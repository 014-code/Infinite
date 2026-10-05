#include "Input.h"

#include "platform/Window.h"

// 按键名称与GLFW编号的转换统一移到Window平台层；Input不再直接依赖底层键值。

Input::Input(const Window &window)
    : window_(window)
{
}

bool Input::isKeyPressed(Key key) const
{
    return isKeyDown(key);
}

bool Input::isKeyDown(Key key) const { return window_.inputState().isKeyDown(key); }
bool Input::wasKeyPressed(Key key) const { return window_.inputState().wasKeyPressed(key); }
bool Input::wasKeyReleased(Key key) const { return window_.inputState().wasKeyReleased(key); }
bool Input::isMouseButtonDown(MouseButton button) const { return window_.inputState().isMouseButtonDown(button); }
bool Input::wasMouseButtonPressed(MouseButton button) const { return window_.inputState().wasMouseButtonPressed(button); }
bool Input::wasMouseButtonReleased(MouseButton button) const { return window_.inputState().wasMouseButtonReleased(button); }
const InputState &Input::inputState() const { return window_.inputState(); }
const glm::dvec2 &Input::mousePosition() const { return window_.inputState().mousePosition(); }
const glm::dvec2 &Input::mouseDelta() const { return window_.inputState().mouseDelta(); }
const glm::dvec2 &Input::scrollDelta() const { return window_.inputState().scrollDelta(); }
