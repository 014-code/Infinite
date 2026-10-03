#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

class Window
{
public:
    // 创建窗口和OpenGL上下文
    Window(int width, int height, const char *title);

    // 对象销毁时关闭窗口并终止GLFW
    ~Window();

    // 窗口资源不能复制，避免重复管理同一个GLFW窗口
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    bool shouldClose() const;
    void swapBuffers() const;
    void pollEvents() const;

private:
    GLFWwindow *window_ = nullptr;
};
