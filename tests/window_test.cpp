#include "TestSupport.h"
#include "input/Input.h"
#include "platform/Window.h"

#include <GL/glew.h>
#include <iostream>

int main()
{
    try
    {
        {
            Window window(64, 48, "Window Test", false);
            Input input(window);
            // Windows窗口装饰可能使实际尺寸大于请求的小尺寸，以GLFW实际值为准。
            int actualWidth = 0, actualHeight = 0;
            glfwGetWindowSize(glfwGetCurrentContext(), &actualWidth, &actualHeight);
            require(window.size() == glm::ivec2(actualWidth, actualHeight) &&
                window.framebufferSize().y > 0, "Invalid dimensions");
            window.setVSyncEnabled(false);
            require(!window.isVSyncEnabled(), "VSync request not stored");
            expectThrow<std::logic_error>([] { Window second(1, 1, "Second", false); }, "Second Window accepted");
            require(glGetString(GL_VERSION) != nullptr, "Failed creation destroyed current context");
            // 通过GLFW公开接口获取已注册回调并馈入测试事件，不发送真实系统键盘事件。
            auto *handle = glfwGetCurrentContext();
            const auto key = glfwSetKeyCallback(handle, nullptr);
            glfwSetKeyCallback(handle, key);
            const auto scroll = glfwSetScrollCallback(handle, nullptr);
            glfwSetScrollCallback(handle, scroll);
            const auto focus = glfwSetWindowFocusCallback(handle, nullptr);
            glfwSetWindowFocusCallback(handle, focus);
            window.pollEvents();
            key(handle, GLFW_KEY_W, 0, GLFW_PRESS, 0);
            key(handle, GLFW_KEY_W, 0, GLFW_RELEASE, 0);
            scroll(handle, 0, 2);
            require(input.wasKeyPressed(Key::W) && input.wasKeyReleased(Key::W) && !input.isKeyDown(Key::W), "Platform lost tap");
            require(input.scrollDelta().y == 2, "Platform lost scroll");
            key(handle, GLFW_KEY_A, 0, GLFW_PRESS, 0);
            focus(handle, GLFW_FALSE);
            require(window.events().focusChanged && !input.isKeyDown(Key::A), "Focus not forwarded");
            window.pollEvents();
            require(!input.wasKeyPressed(Key::W) && input.scrollDelta().y == 0, "Frame not reset");
            expectThrow<std::invalid_argument>([&] { window.waitEvents(0.0); }, "Invalid wait timeout accepted");
            window.requestClose();
            require(window.shouldClose(), "Close flag failed");
        }
        // 前一个窗口完整销毁后允许重新创建。
        Window recreated(32, 32, "Recreated", false);
        expectThrow<std::invalid_argument>([] { Window invalid(0, 1, "Invalid", false); }, "Invalid size accepted");
        require(glGetError() == GL_NO_ERROR, "Window generated GL error");
        std::cout << "Window and input bridge passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
