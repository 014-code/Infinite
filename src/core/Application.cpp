#include "Application.h"

#include "Time.h"
#include "graphics/rendering/OpenGLDebug.h"

#include <cmath>
#include <stdexcept>

namespace
{
    ApplicationConfig checkedConfig(const ApplicationConfig &config)
    {
        if (config.width <= 0 || config.height <= 0)
        {
            throw std::invalid_argument("Application dimensions must be positive");
        }
        if (!std::isfinite(config.exposure) || config.exposure <= 0 || config.exposure > 10000)
        { throw std::invalid_argument("Application exposure must be in (0,10000]"); }
        if (config.directionalShadow)
        {
            config.directionalShadow->validate();
            if (!config.linearHdr) { throw std::invalid_argument("Directional shadows currently require linear HDR/PBR"); }
        }
        for (int channel = 0; channel < 4; ++channel)
        {
            if (!std::isfinite(config.clearColor[channel]))
            {
                throw std::invalid_argument("Application clear color must be finite");
            }
        }
        return config;
    }
}

Application::Application(const ApplicationConfig &config)
    : config_(checkedConfig(config)), timing_(config_.maximumDeltaTime),
      window_(config_.width, config_.height, config_.title.c_str(), config_.visible),
      input_(window_)
{
}

void Application::run(const ApplicationCallbacks &callbacks)
{
    if (started_)
    {
        throw std::logic_error("Application::run can only be called once");
    }
    started_ = true;
    try
    {
        if (callbacks.initialize)
        {
            callbacks.initialize(*this);
        }
        INFINITE_GL_CHECK("application initialization");

        // 初始化完成后才开始计时，避免第一次移动包含Shader编译和图片解码耗时。
        Time clock;
        bool paused = false;
        while (!window_.shouldClose())
        {
            // 暂停时等待事件而不是忙循环。每轮只收集一次事件，避免清掉刚收到的输入边沿。
            if (paused)
            {
                window_.waitEvents(0.05);
            }
            else
            {
                window_.pollEvents();
            }
            if (window_.shouldClose())
            {
                break;
            }
            if (callbacks.onEvents)
            {
                callbacks.onEvents(*this);
            }
            if (window_.shouldClose())
            {
                break;
            }

            const auto size = window_.framebufferSize();
            paused = window_.isMinimized() || size.x <= 0 || size.y <= 0;
            clock.update();
            timing_.advance(clock.deltaTime(), paused);
            if (paused)
            {
                continue;
            }

            const float deltaTime = timing_.frame().deltaTime;
            if (callbacks.update)
            {
                callbacks.update(*this, deltaTime);
            }
            if (window_.shouldClose())
            {
                break;
            }
            scene_.update(deltaTime);
            if (window_.shouldClose())
            {
                break;
            }

            // 清屏同时重置颜色和深度；Scene先画不透明组，再处理透明排序和状态恢复。
            const auto &color = config_.clearColor;
            const auto items = scene_.renderItems();
            std::optional<DirectionalShadowView> shadow;
            if (config_.directionalShadow)
            {
                if (!config_.linearHdr) { throw std::invalid_argument("Directional shadows require linear HDR/PBR"); }
                shadow = shadowMap_.render(items,scene_.lighting().mainLight().direction,*config_.directionalShadow);
            }
            if (config_.linearHdr)
            {
                hdrPipeline_.render(size.x, size.y, color, [&]
                {
                    renderer_.drawItems(items,camera_,window_.aspectRatio(),scene_.lighting(),
                        shadow ? &*shadow : nullptr,true);
                }, config_.exposure);
            }
            else
            {
                renderer_.clear(color.r, color.g, color.b, color.a);
                renderer_.drawItems(items,camera_,window_.aspectRatio(),scene_.lighting());
            }
            if (callbacks.afterRender)
            {
                callbacks.afterRender(*this);
            }
            // 检查本帧累计错误，不假定错误来自最后一条指令；Release移除此调试检查。
            INFINITE_GL_CHECK("application frame");
            if (window_.shouldClose())
            {
                break;
            }
            // 交换缓冲区，将本帧绘制结果呈现到窗口。
            window_.swapBuffers();
        }
    }
    catch (...)
    {
        // Scene::update抛出异常前会恢复自身状态，所以此处可以安全清空物体。
        // 不在析构函数里调用用户清理回调，防止第二次异常覆盖原始错误。
        scene_.clear();
        throw;
    }
    scene_.clear();
}

void Application::requestClose()
{
    window_.requestClose();
}

Window &Application::window()
{
    return window_;
}

ResourceManager &Application::resources()
{
    return resources_;
}

const Input &Application::input() const
{
    return input_;
}

ActionMap &Application::actions()
{
    return actions_;
}

const ActionMap &Application::actions() const
{
    return actions_;
}

Scene &Application::scene()
{
    return scene_;
}

Camera &Application::camera()
{
    return camera_;
}

const FrameTime &Application::frameTime() const
{
    return timing_.frame();
}

DirectionalLight &Application::directionalLight()
{
    return scene_.lighting().mainLight();
}

const DirectionalLight &Application::directionalLight() const
{
    return scene_.lighting().mainLight();
}
