#pragma once

#include "InputState.h"

class Window;

class Input
{
public:
    // 输入系统借用窗口对象读取键盘状态
    explicit Input(const Window &window);
    Input(Window &&) = delete;

    // 查询指定按键当前是否处于按下状态
    bool isKeyPressed(Key key) const;

    // 推荐isKeyDown表示持续按住；旧isKeyPressed保持原语义，避免破坏已有示例。
    bool isKeyDown(Key key) const;
    bool wasKeyPressed(Key key) const;
    bool wasKeyReleased(Key key) const;
    bool isMouseButtonDown(MouseButton button) const;
    bool wasMouseButtonPressed(MouseButton button) const;
    bool wasMouseButtonReleased(MouseButton button) const;
    // ActionMap可以通过这个只读状态入口复用同一帧输入；调用者不能借此修改Window状态。
    const InputState &inputState() const;
    // 鼠标坐标单位为窗口逻辑像素，原点左上角；滚轮/位移在每次pollEvents前清零。
    const glm::dvec2 &mousePosition() const;
    const glm::dvec2 &mouseDelta() const;
    const glm::dvec2 &scrollDelta() const;

private:
    const Window &window_;
};
