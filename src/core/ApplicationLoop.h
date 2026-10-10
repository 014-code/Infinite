#pragma once

#include "FrameTiming.h"
#include "platform/Window.h"

#include <functional>
#include <glm/vec2.hpp>

// ApplicationLoop只负责“什么时候进入一帧”，不负责这一帧要更新哪些引擎服务。
// 具体的输入、物理、Scene、渲染和音频工作通过回调交给Application完成。
struct ApplicationLoopCallbacks
{
    // 窗口事件已经收集后调用。这里可以处理输入、UI事件和状态切换请求。
    std::function<void()> processEvents;
    // 当前帧可以运行时调用；deltaTime已经经过暂停恢复处理和最大值限制。
    std::function<void(float deltaTime)> update;
    // 更新完成且窗口仍然有效时调用。传入的是本帧实际的Framebuffer尺寸。
    std::function<void(const glm::ivec2 &framebufferSize)> render;
};

// 通用桌面应用的帧循环。
//
// 它集中维护几个容易被不同入口重复实现的规则：
// 1. 正常窗口使用pollEvents，最小化窗口使用限时waitEvents，避免后台忙循环。
// 2. 暂停和恢复时由FrameTiming丢弃不应进入游戏模拟的时间。
// 3. update请求关闭后不再进入render，避免在销毁边界上使用已经失效的资源。
//
// 该类不拥有Window和FrameTiming；二者由Application拥有，保证资源生命周期仍由应用统一管理。
class ApplicationLoop final
{
public:
    ApplicationLoop(Window &window, FrameTiming &timing) noexcept;

    ApplicationLoop(const ApplicationLoop &) = delete;
    ApplicationLoop &operator=(const ApplicationLoop &) = delete;

    // 运行到窗口关闭或某个阶段请求关闭为止。回调中的异常会原样向上传播。
    void run(const ApplicationLoopCallbacks &callbacks);

private:
    Window &window_;
    FrameTiming &timing_;
};
