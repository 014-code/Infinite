#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "input/InputState.h"

struct WindowEvents
{
    // 每次pollEvents刷新；同一帧内可以多次读取，不会消费事件。
    bool resized = false;
    bool framebufferResized = false;
    bool focusChanged = false;
    bool minimizedChanged = false;
    bool closeRequested = false;
};

class Window
{
public:
    // 创建窗口和OpenGL上下文。
    // visible为false时创建隐藏窗口，适合运行自动化测试。
    Window(int width, int height, const char *title, bool visible = true);

    // 对象销毁时关闭窗口并终止GLFW
    ~Window();

    // 窗口资源不能复制，避免重复管理同一个GLFW窗口
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    bool shouldClose() const;

    // 当前仅允许一个活动Window，所有窗口/GL操作必须在创建窗口的主线程执行。
    // size是逻辑像素，framebufferSize是实际绘制像素，高DPI时二者可能不同。
    glm::ivec2 size() const;
    glm::ivec2 framebufferSize() const;
    bool isFocused() const;
    bool isMinimized() const;
    void setVSyncEnabled(bool enabled);
    bool isVSyncEnabled() const;
    const WindowEvents &events() const;
    const InputState &inputState() const;

    // 获取窗口当前帧缓冲区的宽高比
    float aspectRatio() const;

    // 查询指定按键当前是否处于按下状态
    bool isKeyPressed(int key) const;

    // 请求关闭窗口，窗口主循环会在下一次判断时结束
    void requestClose() const;

    void swapBuffers() const;
    // 每帧调用一次：先清除输入边沿/位移和事件，再收集本帧事件。
    void pollEvents();
    // 暂停时使用：和pollEvents一样先刷新本帧输入，再限时等待，不能在同一轮重复调用两者。
    // timeoutSeconds必须有限且大于0；事件到达时会提前返回。
    void waitEvents(double timeoutSeconds);

private:
    GLFWwindow *window_ = nullptr;
    InputState inputState_;
    WindowEvents events_;
    bool vSyncEnabled_ = true;
    bool minimized_ = false;
};
