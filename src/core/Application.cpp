#include "Application.h"

#include "Time.h"
#include "game/state/ApplicationStateStack.h"
#include "graphics/rendering/OpenGLDebug.h"
#include "ui/UiRenderer.h"

#include <cmath>
#include <memory>
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
      input_(window_), audio_(config_.audio)
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
            StateFramePolicy policy;
            if (!processEvents(callbacks, paused, policy))
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
            updateFrame(callbacks, policy, deltaTime);
            if (window_.shouldClose())
            {
                break;
            }

            renderFrame(callbacks, policy, size);
        }
    }
    catch (...)
    {
        // Scene::update抛出异常前会恢复自身状态，所以此处可以安全卸载当前关卡。
        // 通过SceneManager清理也会同步currentName_，避免应用退出时留下错误的管理状态。
        // 不在析构函数里调用用户清理回调，防止第二次异常覆盖原始错误。
        if (callbacks.shutdown)
        {
            try { callbacks.shutdown(*this); }
            catch (...) { /* 保留主循环原始异常。 */ }
        }
        sceneManager_.unload();
        throw;
    }
    if (callbacks.shutdown)
    {
        callbacks.shutdown(*this);
    }
    sceneManager_.unload();
}

bool Application::processEvents(const ApplicationCallbacks &callbacks, bool paused,
    StateFramePolicy &policy)
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
        return false;
    }

    if (callbacks.onEvents)
    {
        callbacks.onEvents(*this);
    }
    // UI在应用事件回调之后处理本帧输入，按钮回调可以安全地请求状态切换；
    // 状态切换本身仍由ApplicationStateStack在下一帧安全边界提交。
    ui_.processInput(input_.inputState(), glm::vec2(window_.size()));
    // 关卡切换只在事件阶段提交，避免清空Scene时仍处于Scene::update遍历中。
    sceneManager_.commitPending();
    if (window_.shouldClose())
    {
        return false;
    }

    // 必须在事件和UI回调完成后读取策略，保证本帧使用的是当前应用状态的配置。
    policy = callbacks.framePolicy
        ? callbacks.framePolicy(*this)
        : StateFramePolicy{};
    return true;
}

void Application::updateFrame(const ApplicationCallbacks &callbacks,
    const StateFramePolicy &policy, float deltaTime)
{
    if (callbacks.update)
    {
        callbacks.update(*this, deltaTime);
    }
    if (window_.shouldClose())
    {
        return;
    }

    float interpolationAlpha = 0.0f;
    if (policy.simulatePhysics)
    {
        physicsWorld_.step(deltaTime, [&](float fixedDeltaTime)
        {
            // 静态碰撞体先读取本帧应用逻辑写入的Transform。
            scene_.syncStaticPhysics(physicsWorld_);
            if (callbacks.fixedUpdate)
            {
                callbacks.fixedUpdate(*this, fixedDeltaTime);
            }
            // fixedUpdate可能移动了静态物体，再同步一次确保本子步看到最新位姿。
            scene_.syncStaticPhysics(physicsWorld_);
        });
        interpolationAlpha = physicsWorld_.interpolationAlpha();
        scene_.syncDynamicPhysics(physicsWorld_, interpolationAlpha);
        scene_.syncCharacterPhysics();
        scene_.updateAreas();
        if (callbacks.afterPhysics)
        {
            callbacks.afterPhysics(*this, interpolationAlpha);
        }
    }
    else
    {
        // 暂停期间不保留旧的半步时间，恢复后从新的帧边界继续模拟。
        physicsWorld_.resetAccumulator();
    }
    if (policy.updateScene)
    {
        scene_.update(deltaTime);
    }
    scene_.syncAudio();
    // 音频状态在Scene更新后推进，保证后续接入AudioSourceComponent时
    // 能够读取到本帧最新的Transform；音频不参与固定物理子步。
    if (policy.updateAudio)
    {
        audioEvents_.dispatch(audio_, music_);
        music_.update(deltaTime);
        audio_.update(deltaTime);
    }
}

void Application::renderFrame(const ApplicationCallbacks &callbacks,
    const StateFramePolicy &policy, const glm::ivec2 &framebufferSize)
{
    // 清屏同时重置颜色和深度；Scene先画不透明组，再处理透明排序和状态恢复。
    const auto &color = config_.clearColor;
    const auto items = policy.renderScene ? scene_.renderItems() : std::vector<RenderItem>{};
    std::optional<DirectionalShadowView> shadow;
    if (config_.directionalShadow)
    {
        if (!config_.linearHdr) { throw std::invalid_argument("Directional shadows require linear HDR/PBR"); }
        shadow = shadowMap_.render(items,scene_.lighting().mainLight().direction,*config_.directionalShadow);
    }
    if (config_.linearHdr)
    {
        hdrPipeline_.render(framebufferSize.x, framebufferSize.y, color, [&]
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
    if (!ui_.empty())
    {
        if (!uiRenderer_)
        {
            uiRenderer_ = std::make_unique<UiRenderer>(config_.uiFontPath);
        }
        uiRenderer_->render(ui_, window_.size(), framebufferSize);
    }
    if (callbacks.afterRender)
    {
        callbacks.afterRender(*this);
    }
    // 检查本帧累计错误，不假定错误来自最后一条指令；Release移除此调试检查。
    INFINITE_GL_CHECK("application frame");
    if (window_.shouldClose())
    {
        return;
    }
    // 交换缓冲区，将本帧绘制结果呈现到窗口。
    window_.swapBuffers();
}

void Application::run(ApplicationStateStack &states)
{
    ApplicationCallbacks callbacks;
    callbacks.initialize = [&states](Application &application)
    {
        states.start(application);
    };
    callbacks.onEvents = [&states](Application &)
    {
        // 上一帧的请求在本帧事件阶段开始前提交，避免回调执行中销毁当前状态。
        states.commitPending();
        states.dispatchEvents();
    };
    callbacks.update = [&states](Application &, float deltaTime)
    {
        states.dispatchUpdate(deltaTime);
    };
    callbacks.framePolicy = [&states](Application &)
    {
        return states.framePolicy();
    };
    callbacks.fixedUpdate = [&states](Application &, float fixedDeltaTime)
    {
        states.dispatchFixedUpdate(fixedDeltaTime);
    };
    callbacks.afterPhysics = [&states](Application &, float interpolationAlpha)
    {
        states.dispatchAfterPhysics(interpolationAlpha);
    };
    callbacks.afterRender = [&states](Application &)
    {
        states.dispatchAfterRender();
    };
    callbacks.shutdown = [&states](Application &)
    {
        states.stop();
    };
    run(callbacks);
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

AudioSystem &Application::audio()
{
    return audio_;
}

const AudioSystem &Application::audio() const
{
    return audio_;
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

PhysicsWorld &Application::physicsWorld()
{
    return physicsWorld_;
}

const PhysicsWorld &Application::physicsWorld() const
{
    return physicsWorld_;
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
