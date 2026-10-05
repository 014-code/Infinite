#include "InputState.h"

#include <cmath>

void InputState::beginFrame()
{
    // GLFW回调可能在一帧内触发多次，但pressed/released只表示“本帧发生过”。
    // 因此每次pollEvents或waitEvents前清掉上一帧的边沿，按住状态则继续保留。
    for (auto &key : keys_) { key.pressed = false; key.released = false; }
    for (auto &button : buttons_) { button.pressed = false; button.released = false; }
    mouseDelta_ = glm::dvec2(0.0);
    scrollDelta_ = glm::dvec2(0.0);
}

void InputState::updateButton(Button &button, bool down)
{
    // 自动重复按键不重复产生pressed；释放未按下的键也不制造假边沿。
    if (down && !button.down) { button.pressed = true; }
    if (!down && button.down) { button.released = true; }
    button.down = down;
}

void InputState::keyEvent(Key key, bool down)
{
    // Window已经把GLFW按键映射成引擎自己的Key；越界值直接忽略，
    // 这样未知平台按键不会破坏固定大小的状态数组。
    const auto index = static_cast<std::size_t>(key);
    if (index < keys_.size()) { updateButton(keys_[index], down); }
}

void InputState::mouseButtonEvent(MouseButton button, bool down)
{
    const auto index = static_cast<std::size_t>(button);
    if (index < buttons_.size()) { updateButton(buttons_[index], down); }
}

void InputState::cursorEvent(double x, double y)
{
    if (!std::isfinite(x) || !std::isfinite(y)) { return; }
    const glm::dvec2 position(x, y);
    // 初次收到位置、或重新获得焦点后，不把绝对坐标当成本帧的大幅移动。
    // 后续只累计位移，不要求上层每次都读取绝对鼠标坐标。
    if (hasCursor_) { mouseDelta_ += position - mousePosition_; }
    mousePosition_ = position;
    hasCursor_ = true;
}

void InputState::scrollEvent(double x, double y)
{
    if (std::isfinite(x) && std::isfinite(y)) { scrollDelta_ += glm::dvec2(x, y); }
}

void InputState::focusLost()
{
    // 失焦时系统可能不会再发送对应的松键事件，所以主动释放全部输入。
    // 同时清空鼠标基准，重新获得焦点后的第一帧不会产生跳跃位移。
    for (auto &key : keys_) { updateButton(key, false); }
    for (auto &button : buttons_) { updateButton(button, false); }
    hasCursor_ = false;
    mouseDelta_ = glm::dvec2(0.0);
    scrollDelta_ = glm::dvec2(0.0);
}

bool InputState::isKeyDown(Key key) const
{
    const auto index = static_cast<std::size_t>(key);
    return index < keys_.size() && keys_[index].down;
}

bool InputState::wasKeyPressed(Key key) const
{
    const auto index = static_cast<std::size_t>(key);
    return index < keys_.size() && keys_[index].pressed;
}

bool InputState::wasKeyReleased(Key key) const
{
    const auto index = static_cast<std::size_t>(key);
    return index < keys_.size() && keys_[index].released;
}

bool InputState::isMouseButtonDown(MouseButton button) const
{
    const auto index = static_cast<std::size_t>(button);
    return index < buttons_.size() && buttons_[index].down;
}

bool InputState::wasMouseButtonPressed(MouseButton button) const
{
    const auto index = static_cast<std::size_t>(button);
    return index < buttons_.size() && buttons_[index].pressed;
}

bool InputState::wasMouseButtonReleased(MouseButton button) const
{
    const auto index = static_cast<std::size_t>(button);
    return index < buttons_.size() && buttons_[index].released;
}

const glm::dvec2 &InputState::mousePosition() const { return mousePosition_; }
const glm::dvec2 &InputState::mouseDelta() const { return mouseDelta_; }
const glm::dvec2 &InputState::scrollDelta() const { return scrollDelta_; }
