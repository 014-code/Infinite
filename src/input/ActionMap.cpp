#include "ActionMap.h"

#include "Input.h"

#include <cstddef>
#include <stdexcept>

namespace
{
    template <typename Enum>
    bool isValidEnum(Enum value, Enum count) noexcept
    {
        return static_cast<std::size_t>(value) < static_cast<std::size_t>(count);
    }
}

void ActionMap::createAction(const std::string &name)
{
    validateActionName(name);
    actions_.try_emplace(name);
}

bool ActionMap::removeAction(const std::string &name) noexcept
{
    return actions_.erase(name) != 0;
}

bool ActionMap::hasAction(const std::string &name) const noexcept
{
    return actions_.find(name) != actions_.end();
}

void ActionMap::bindKey(const std::string &action, Key key)
{
    validateKey(key);
    auto &bindings = actions_.at(action);
    const Binding candidate{BindingType::Key, key, MouseButton::Left};
    for (const Binding &binding : bindings)
    {
        if (sameBinding(binding, candidate)) { return; }
    }
    bindings.push_back(candidate);
}

void ActionMap::bindMouseButton(const std::string &action, MouseButton button)
{
    validateMouseButton(button);
    auto &bindings = actions_.at(action);
    const Binding candidate{BindingType::MouseButton, Key::A, button};
    for (const Binding &binding : bindings)
    {
        if (sameBinding(binding, candidate)) { return; }
    }
    bindings.push_back(candidate);
}

void ActionMap::clearBindings(const std::string &action)
{
    bindingsFor(action);
    actions_.at(action).clear();
}

bool ActionMap::isDown(const std::string &action, const InputState &state) const
{
    const auto &bindings = bindingsFor(action);
    for (const Binding &binding : bindings)
    {
        if (binding.type == BindingType::Key && state.isKeyDown(binding.key)) { return true; }
        if (binding.type == BindingType::MouseButton && state.isMouseButtonDown(binding.mouseButton))
        {
            return true;
        }
    }
    return false;
}

bool ActionMap::wasPressed(const std::string &action, const InputState &state) const
{
    const auto &bindings = bindingsFor(action);
    for (const Binding &binding : bindings)
    {
        if (binding.type == BindingType::Key && state.wasKeyPressed(binding.key)) { return true; }
        if (binding.type == BindingType::MouseButton && state.wasMouseButtonPressed(binding.mouseButton))
        {
            return true;
        }
    }
    return false;
}

bool ActionMap::wasReleased(const std::string &action, const InputState &state) const
{
    const auto &bindings = bindingsFor(action);
    for (const Binding &binding : bindings)
    {
        if (binding.type == BindingType::Key && state.wasKeyReleased(binding.key)) { return true; }
        if (binding.type == BindingType::MouseButton && state.wasMouseButtonReleased(binding.mouseButton))
        {
            return true;
        }
    }
    return false;
}

bool ActionMap::isDown(const std::string &action, const Input &input) const
{
    return isDown(action, input.inputState());
}

bool ActionMap::wasPressed(const std::string &action, const Input &input) const
{
    return wasPressed(action, input.inputState());
}

bool ActionMap::wasReleased(const std::string &action, const Input &input) const
{
    return wasReleased(action, input.inputState());
}

const std::vector<ActionMap::Binding> &ActionMap::bindingsFor(const std::string &action) const
{
    const auto found = actions_.find(action);
    if (found == actions_.end())
    {
        throw std::out_of_range("Unknown input action: " + action);
    }
    return found->second;
}

void ActionMap::validateActionName(const std::string &name)
{
    if (name.empty())
    {
        throw std::invalid_argument("Input action name must not be empty");
    }
}

void ActionMap::validateKey(Key key)
{
    if (!isValidEnum(key, Key::Count))
    {
        throw std::invalid_argument("Invalid input action key");
    }
}

void ActionMap::validateMouseButton(MouseButton button)
{
    if (!isValidEnum(button, MouseButton::Count))
    {
        throw std::invalid_argument("Invalid input action mouse button");
    }
}

bool ActionMap::sameBinding(const Binding &left, const Binding &right) noexcept
{
    if (left.type != right.type) { return false; }
    if (left.type == BindingType::Key) { return left.key == right.key; }
    return left.mouseButton == right.mouseButton;
}
