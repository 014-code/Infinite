#pragma once

#include "PlayerCommand.h"
#include "input/ActionMap.h"

#include <set>
#include <stdexcept>
#include <string>

namespace PlayerExample
{
    struct PlayerBindings
    {
        std::string forward = "move_forward";
        std::string backward = "move_backward";
        std::string left = "move_left";
        std::string right = "move_right";
        std::string jump = "jump";
        std::string sprint = "sprint";
    };

    // 动作名校验集中在示例配置阶段。先全部校验，再注册，避免无效配置留下半组绑定。
    inline void validateBindings(const PlayerBindings &bindings)
    {
        std::set<std::string> names;
        for (const auto *name : {&bindings.forward, &bindings.backward, &bindings.left,
            &bindings.right, &bindings.jump, &bindings.sprint})
        {
            if (name->empty() || !names.insert(*name).second)
            {
                throw std::invalid_argument("Player example action names must be nonempty and distinct");
            }
        }
    }

    // 由main显式调用；创建移动规则对象不再偷偷修改ActionMap。
    // 已存在动作完全保留（包括空绑定），便于示例改键；重复注册也不会追加默认键。
    inline void registerDefaultActions(ActionMap &actions, const PlayerBindings &bindings = {})
    {
        validateBindings(bindings);
        const auto ensure = [&](const std::string &name, Key key)
        {
            if (!actions.hasAction(name))
            {
                actions.createAction(name);
                actions.bindKey(name, key);
            }
        };
        ensure(bindings.forward, Key::W);
        ensure(bindings.backward, Key::S);
        ensure(bindings.left, Key::A);
        ensure(bindings.right, Key::D);
        ensure(bindings.jump, Key::Space);
        if (!actions.hasAction(bindings.sprint))
        {
            ensure(bindings.sprint, Key::LeftShift);
            actions.bindKey(bindings.sprint, Key::RightShift);
        }
    }

    // Input和纯CPU InputState共用同一查询逻辑。这里只读取，不创建或修改动作。
    // 失焦时发空指令，阻止操作；移动规则继续受重力影响，不因输入焦点而冻结。
    template<class InputSource>
    PlayerCommand sampleCommand(const ActionMap &actions, const InputSource &input,
        const PlayerBindings &bindings = {}, bool focused = true)
    {
        if (!focused) { return {}; }
        PlayerCommand command;
        command.movement = {
            float(actions.isDown(bindings.right, input)) - float(actions.isDown(bindings.left, input)),
            float(actions.isDown(bindings.backward, input)) - float(actions.isDown(bindings.forward, input))};
        command.jumpPressed = actions.wasPressed(bindings.jump, input);
        command.sprint = actions.isDown(bindings.sprint, input);
        return command;
    }
}
