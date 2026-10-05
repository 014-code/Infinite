#pragma once

#include <array>
#include <cstddef>
#include <glm/vec2.hpp>

// 引擎自己的按键编号，不暴露GLFW常量。连续区间方便平台层映射常用键。
enum class Key
{
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Escape, Space, Enter, Tab, Backspace, Left, Right, Up, Down,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,
    Count
};

enum class MouseButton { Left, Right, Middle, Count };

// 纯CPU事件状态：Window写入，Input读取；测试可以直接馈入事件而不模拟系统键盘。
// 每帧先beginFrame，再接收事件。同一帧按下并松开时，两个边沿都会保留。
class InputState
{
public:
    void beginFrame();
    void keyEvent(Key key, bool down);
    void mouseButtonEvent(MouseButton button, bool down);
    void cursorEvent(double x, double y);
    void scrollEvent(double x, double y);
    void focusLost();

    bool isKeyDown(Key key) const;
    bool wasKeyPressed(Key key) const;
    bool wasKeyReleased(Key key) const;
    bool isMouseButtonDown(MouseButton button) const;
    bool wasMouseButtonPressed(MouseButton button) const;
    bool wasMouseButtonReleased(MouseButton button) const;
    const glm::dvec2 &mousePosition() const;
    const glm::dvec2 &mouseDelta() const;
    const glm::dvec2 &scrollDelta() const;

private:
    struct Button { bool down = false; bool pressed = false; bool released = false; };
    static void updateButton(Button &button, bool down);
    std::array<Button, static_cast<std::size_t>(Key::Count)> keys_{};
    std::array<Button, static_cast<std::size_t>(MouseButton::Count)> buttons_{};
    glm::dvec2 mousePosition_{0.0};
    glm::dvec2 mouseDelta_{0.0};
    glm::dvec2 scrollDelta_{0.0};
    bool hasCursor_ = false;
};
