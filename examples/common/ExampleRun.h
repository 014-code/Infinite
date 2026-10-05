#pragma once

#include "core/Application.h"
#include "core/Log.h"
#include "core/ResourcePath.h"
#include "graphics/rendering/OpenGLDebug.h"

#include <GL/glew.h>
#include <array>
#include <cmath>
#include <stdexcept>
#include <filesystem>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

enum class ExampleFrameContent
{
    DrawnScene,
    ClearOnly
};

// 只属于示例层的运行选项，不把测试帧数、隐藏演示窗口等策略放进引擎。
class ExampleRun
{
public:
    ExampleRun(int argc, char *argv[], ExampleFrameContent content = ExampleFrameContent::DrawnScene)
        : content_(content)
    {
        if (argc == 2 && std::string_view(argv[1]) == "--smoke-test")
        {
            smoke_ = true;
        }
        else if (argc != 1)
        {
            throw std::invalid_argument("Usage: example [--smoke-test]");
        }
    }

    bool visible() const
    {
        return !smoke_;
    }

    // 通用的示例策略仍在应用层：Escape退出、三帧冒烟都不是引擎的固定规则。
    ApplicationCallbacks callbacks()
    {
        ApplicationCallbacks result;
        result.onEvents = [](Application &application)
        {
            // 按下ESC键时请求关闭窗口，本轮不再继续更新或绘制。
            if (application.input().isKeyDown(Key::Escape))
            {
                application.requestClose();
            }
        };
        result.afterRender = [this](Application &application)
        {
            verifyFrame();
            if (smoke_ && ++frames_ >= 3)
            {
                application.requestClose();
            }
        };
        return result;
    }

    void verifyFrame() const
    {
        if (!smoke_)
        {
            return;
        }
        // 读回实际帧缓冲，确认不仅创建成功，也确实有预期的绘制/清屏结果。
        // 不只检查一个中心像素：模型可能位于中心附近，而中心点恰好落在暗色材质、
        // 空洞或背景上。整帧扫描仍然只判断是否出现了真实的非清屏像素。
        std::array<GLint, 4> viewport{};
        std::array<GLfloat, 4> clear{};
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear.data());
        if (viewport[2] <= 0 || viewport[3] <= 0 ||
            static_cast<std::size_t>(viewport[2]) >
                std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(viewport[3]))
        {
            throw std::runtime_error("Example smoke test received an invalid viewport");
        }
        const std::size_t pixelCount = static_cast<std::size_t>(viewport[2]) *
            static_cast<std::size_t>(viewport[3]);
        if (pixelCount > std::numeric_limits<std::size_t>::max() / 4)
        {
            throw std::runtime_error("Example smoke test framebuffer is too large");
        }
        std::vector<unsigned char> pixels(pixelCount * 4);
        glReadPixels(viewport[0], viewport[1], viewport[2], viewport[3],
            GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        bool changed = false;
        for (std::size_t pixel = 0; pixel < pixelCount && !changed; ++pixel)
        {
            for (int channel = 0; channel < 3; ++channel)
            {
                changed = std::abs(pixels[pixel * 4 + static_cast<std::size_t>(channel)] -
                    clear[channel] * 255.0f) > 3;
                if (changed) { break; }
            }
        }
        const bool expectContent = content_ == ExampleFrameContent::DrawnScene;
        if (!OpenGLDebug::checkErrors("example smoke frame", __FILE__, __LINE__) || changed != expectContent)
        {
            throw std::runtime_error("Example smoke test failed: GL error or unexpected frame content");
        }
    }

private:
    bool smoke_ = false;
    int frames_ = 0;
    ExampleFrameContent content_;
};

// 启动日志、资源目录、异常出口是示例共同的样板，不要求每个main重复写一份。
// initialize只创建本例内容；Application负责事件、更新、清屏、Scene绘制与交换缓冲。
inline int runExample(int argc, char *argv[], const char *name, const char *title,
    const glm::vec4 &clearColor,
    const std::function<void(Application &, const std::filesystem::path &)> &initialize,
    ExampleFrameContent content = ExampleFrameContent::DrawnScene,
    const std::function<void(Application &, float)> &update = {})
{
    try
    {
        ExampleRun run(argc, argv, content);
        // 在窗口创建前打开日志，启动失败也能留下记录；路径不依赖当前工作目录。
        const auto directory = executableDirectory(argv[0]);
        if (!Log::setFile(directory / "logs" / (std::string(name) + ".log")))
        {
            LOG_WARN("Cannot open log file; console logging remains available");
        }
        LOG_INFO(std::string("Starting ") + name + " example (Escape exits)");
        ApplicationConfig config;
        config.title = title;
        config.visible = run.visible();
        config.clearColor = clearColor;
        Application application(config);
        auto callbacks = run.callbacks();
        callbacks.initialize = [&](Application &app)
        {
            initialize(app, directory);
        };
        callbacks.update = update;
        application.run(callbacks);
        LOG_INFO(std::string(name) + " example stopped");
        return 0;
    }
    catch (const std::exception &exception)
    {
        // run抛出前已经清空Scene；Application析构后窗口关闭，入口只负责记录失败。
        LOG_ERROR(exception.what());
        return 1;
    }
}
