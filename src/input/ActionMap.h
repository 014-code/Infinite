#pragma once

#include "InputState.h"

#include <string>
#include <unordered_map>
#include <vector>

class Input;

// ActionMap把具体按键转换为应用层动作名称，例如把W和上方向键都绑定到
// "move_forward"。它不轮询窗口、不接收GLFW回调，只读取调用者提供的当前输入状态。
// 一个动作的多个绑定采用OR语义：任意一个绑定满足条件，动作查询就返回true。
class ActionMap final
{
public:
    // 动作名称必须非空；重复创建同名动作是幂等操作，不会清掉已有绑定。
    void createAction(const std::string &name);
    bool removeAction(const std::string &name) noexcept;
    bool hasAction(const std::string &name) const noexcept;

    // 绑定未知动作或非法枚举值会抛出异常。重复绑定相同输入不会添加重复项。
    void bindKey(const std::string &action, Key key);
    void bindMouseButton(const std::string &action, MouseButton button);
    void clearBindings(const std::string &action);

    // 查询未知动作会抛出std::out_of_range，避免拼写错误静默失效。
    bool isDown(const std::string &action, const InputState &state) const;
    bool wasPressed(const std::string &action, const InputState &state) const;
    bool wasReleased(const std::string &action, const InputState &state) const;

    // 应用层便捷入口：Input只是Window输入状态的只读外观，不会改变查询语义。
    bool isDown(const std::string &action, const Input &input) const;
    bool wasPressed(const std::string &action, const Input &input) const;
    bool wasReleased(const std::string &action, const Input &input) const;

private:
    enum class BindingType { Key, MouseButton };

    struct Binding
    {
        BindingType type;
        Key key = Key::A;
        MouseButton mouseButton = MouseButton::Left;
    };

    const std::vector<Binding> &bindingsFor(const std::string &action) const;
    static void validateActionName(const std::string &name);
    static void validateKey(Key key);
    static void validateMouseButton(MouseButton button);
    static bool sameBinding(const Binding &left, const Binding &right) noexcept;

    std::unordered_map<std::string, std::vector<Binding>> actions_;
};
