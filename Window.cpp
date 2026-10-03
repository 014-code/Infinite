#include "Window.h"

#include <GL/glew.h>

#include <stdexcept>

Window::Window(int width, int height, const char *title)
{
    // 初始化GLFW
    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // 设置OpenGL版本
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 创建窗口
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    // 将窗口的上下文设置为当前线程的上下文
    glfwMakeContextCurrent(window_);

    // 初始化GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLEW");
    }
}

// Window对象销毁时，释放GLFW窗口并终止GLFW
Window::~Window()
{
    if (window_ != nullptr)
    {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(window_);
}

void Window::swapBuffers() const
{
    glfwSwapBuffers(window_);
}

void Window::pollEvents() const
{
    glfwPollEvents();
}
