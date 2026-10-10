#include "ApplicationLoop.h"

#include "Time.h"

ApplicationLoop::ApplicationLoop(Window &window, FrameTiming &timing) noexcept
    : window_(window), timing_(timing)
{
}

void ApplicationLoop::run(const ApplicationLoopCallbacks &callbacks)
{
    // 初始化资源的耗时不应进入第一帧，因此时钟在真正开始循环时才创建。
    Time clock;
    bool paused = false;

    while (!window_.shouldClose())
    {
        // 最小化窗口没有可绘制的Framebuffer，使用等待事件可以避免持续占用CPU。
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

        if (callbacks.processEvents)
        {
            callbacks.processEvents();
        }
        if (window_.shouldClose())
        {
            break;
        }

        const glm::ivec2 framebufferSize = window_.framebufferSize();
        paused = window_.isMinimized() || framebufferSize.x <= 0 || framebufferSize.y <= 0;

        clock.update();
        timing_.advance(clock.deltaTime(), paused);
        if (paused)
        {
            continue;
        }

        if (callbacks.update)
        {
            callbacks.update(timing_.frame().deltaTime);
        }
        if (window_.shouldClose())
        {
            break;
        }

        if (callbacks.render)
        {
            callbacks.render(framebufferSize);
        }
    }
}
