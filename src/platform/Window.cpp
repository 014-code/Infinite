#include "Window.h"

#include <GL/glew.h>

#include <stdexcept>
#include <cmath>
#include <string>

namespace
{
    bool hasActiveWindow = false;

    // 平台层负责将GLFW编号转换为引擎按键；未知键映射为Count并被状态容器忽略。
    Key engineKey(int key)
    {
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) { return static_cast<Key>(key - GLFW_KEY_A); }
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) { return static_cast<Key>(static_cast<int>(Key::Num0) + key - GLFW_KEY_0); }
        if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) { return static_cast<Key>(static_cast<int>(Key::F1) + key - GLFW_KEY_F1); }
        switch (key)
        {
        case GLFW_KEY_ESCAPE: return Key::Escape;
        case GLFW_KEY_SPACE: return Key::Space;
        case GLFW_KEY_ENTER: return Key::Enter;
        case GLFW_KEY_TAB: return Key::Tab;
        case GLFW_KEY_BACKSPACE: return Key::Backspace;
        case GLFW_KEY_LEFT: return Key::Left;
        case GLFW_KEY_RIGHT: return Key::Right;
        case GLFW_KEY_UP: return Key::Up;
        case GLFW_KEY_DOWN: return Key::Down;
        case GLFW_KEY_LEFT_SHIFT: return Key::LeftShift;
        case GLFW_KEY_RIGHT_SHIFT: return Key::RightShift;
        case GLFW_KEY_LEFT_CONTROL: return Key::LeftControl;
        case GLFW_KEY_RIGHT_CONTROL: return Key::RightControl;
        case GLFW_KEY_LEFT_ALT: return Key::LeftAlt;
        case GLFW_KEY_RIGHT_ALT: return Key::RightAlt;
        default: return Key::Count;
        }
    }
    std::runtime_error glfwFailure(const char *operation)
    {
        // 在终止GLFW前读取底层原因；只抛异常，由应用入口统一记录日志。
        const char *description = nullptr;
        const int code = glfwGetError(&description);
        return std::runtime_error(std::string(operation) + " (GLFW " + std::to_string(code)
            + "): " + (description == nullptr ? "No error description" : description));
    }
}

Window::Window(int width, int height, const char *title, bool visible)
{
    if (width <= 0 || height <= 0 || title == nullptr)
    {
        throw std::invalid_argument("Window requires positive dimensions and a title");
    }
    // 在触碰GLFW之前拒绝第二个窗口，失败构造不能终止已有窗口的上下文。
    if (hasActiveWindow) { throw std::logic_error("Only one active Window is supported"); }
    // 初始化GLFW
    if (!glfwInit())
    {
        throw glfwFailure("Failed to initialize GLFW");
    }

    // 设置OpenGL版本
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // 显式请求24位深度缓冲，供深度测试使用。
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    // 测试场景不需要显示窗口，但仍然需要创建OpenGL上下文。
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);

    // 创建窗口
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_)
    {
        const auto error = glfwFailure("Failed to create GLFW window");
        glfwTerminate();
        throw error;
    }

    // 将窗口的上下文设置为当前线程的上下文
    glfwMakeContextCurrent(window_);

    // 初始化GLEW
    glewExperimental = GL_TRUE;
    const GLenum glewResult = glewInit();
    if (glewResult != GLEW_OK)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        throw std::runtime_error(std::string("Failed to initialize GLEW: ")
            + reinterpret_cast<const char *>(glewGetErrorString(glewResult)));
    }

    // 初始化OpenGL视口，并在窗口尺寸变化时同步更新
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow *handle, int width, int height)
    {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(handle));
        self.events_.framebufferResized = true;
        glViewport(0, 0, width, height);
    });
    glfwSetWindowSizeCallback(window_, [](GLFWwindow *handle, int, int)
    {
        static_cast<Window *>(glfwGetWindowUserPointer(handle))->events_.resized = true;
    });
    glfwSetWindowFocusCallback(window_, [](GLFWwindow *handle, int focused)
    {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(handle));
        self.events_.focusChanged = true;
        if (!focused) { self.inputState_.focusLost(); }
    });
    minimized_ = glfwGetWindowAttrib(window_, GLFW_ICONIFIED) == GLFW_TRUE;
    glfwSetWindowIconifyCallback(window_, [](GLFWwindow *handle, int minimized)
    {
        auto &self = *static_cast<Window *>(glfwGetWindowUserPointer(handle));
        self.minimized_ = minimized == GLFW_TRUE;
        self.events_.minimizedChanged = true;
    });
    glfwSetWindowCloseCallback(window_, [](GLFWwindow *handle)
    {
        static_cast<Window *>(glfwGetWindowUserPointer(handle))->events_.closeRequested = true;
    });
    glfwSetKeyCallback(window_, [](GLFWwindow *handle, int key, int, int action, int)
    {
        if (action != GLFW_REPEAT)
        {
            static_cast<Window *>(glfwGetWindowUserPointer(handle))->inputState_.keyEvent(engineKey(key), action == GLFW_PRESS);
        }
    });
    glfwSetMouseButtonCallback(window_, [](GLFWwindow *handle, int button, int action, int)
    {
        const auto mapped = button == GLFW_MOUSE_BUTTON_LEFT ? MouseButton::Left :
            button == GLFW_MOUSE_BUTTON_RIGHT ? MouseButton::Right :
            button == GLFW_MOUSE_BUTTON_MIDDLE ? MouseButton::Middle : MouseButton::Count;
        static_cast<Window *>(glfwGetWindowUserPointer(handle))->inputState_.mouseButtonEvent(mapped, action == GLFW_PRESS);
    });
    glfwSetCursorPosCallback(window_, [](GLFWwindow *handle, double x, double y)
    {
        static_cast<Window *>(glfwGetWindowUserPointer(handle))->inputState_.cursorEvent(x, y);
    });
    glfwSetScrollCallback(window_, [](GLFWwindow *handle, double x, double y)
    {
        static_cast<Window *>(glfwGetWindowUserPointer(handle))->inputState_.scrollEvent(x, y);
    });
    hasActiveWindow = true;
    setVSyncEnabled(true);
}

// Window对象销毁时，释放GLFW窗口并终止GLFW
Window::~Window()
{
    if (window_ != nullptr)
    {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
    hasActiveWindow = false;
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(window_);
}

float Window::aspectRatio() const
{
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

    if (framebufferWidth == 0 || framebufferHeight == 0)
    {
        return 1.0f;
    }

    return static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
}

// 查询键盘当前状态，适合处理WASD这类持续输入
bool Window::isKeyPressed(int key) const
{
    return inputState_.isKeyDown(engineKey(key));
}

// 设置窗口关闭标记，让主循环正常退出
void Window::requestClose() const
{
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

void Window::swapBuffers() const
{
    glfwSwapBuffers(window_);
}

void Window::pollEvents()
{
    inputState_.beginFrame();
    events_ = {};
    glfwPollEvents();
}

void Window::waitEvents(double timeoutSeconds)
{
    if (!std::isfinite(timeoutSeconds) || timeoutSeconds <= 0.0)
    {
        throw std::invalid_argument("Event wait timeout must be positive and finite");
    }
    inputState_.beginFrame();
    events_ = {};
    glfwWaitEventsTimeout(timeoutSeconds);
}

glm::ivec2 Window::size() const
{
    glm::ivec2 result;
    glfwGetWindowSize(window_, &result.x, &result.y);
    return result;
}

glm::ivec2 Window::framebufferSize() const
{
    glm::ivec2 result;
    glfwGetFramebufferSize(window_, &result.x, &result.y);
    return result;
}

bool Window::isFocused() const { return glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE; }
bool Window::isMinimized() const
{
    // 状态由GLFW事件回调更新，和本轮输入及WindowEvents保持一致。
    return minimized_;
}

void Window::setVSyncEnabled(bool enabled)
{
    // 保存的是应用请求，最终是否等待刷新仍由显示驱动策略决定。
    glfwSwapInterval(enabled ? 1 : 0);
    vSyncEnabled_ = enabled;
}

bool Window::isVSyncEnabled() const { return vSyncEnabled_; }
const WindowEvents &Window::events() const { return events_; }
const InputState &Window::inputState() const { return inputState_; }
