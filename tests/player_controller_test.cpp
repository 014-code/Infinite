#include "TestSupport.h"

#include "player_controller/FlatGroundMovement.h"
#include "player_controller/PlayerInput.h"
#include "math/Transform.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    using namespace PlayerExample;

    bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }

    void testInput()
    {
        ActionMap actions;
        FlatGroundMovement movement;
        require(!actions.hasAction("move_forward"), "Movement construction changed input bindings");
        registerDefaultActions(actions);
        InputState input;
        input.keyEvent(Key::W, true);
        input.keyEvent(Key::D, true);
        input.keyEvent(Key::RightShift, true);
        input.keyEvent(Key::Space, true);
        const auto command = sampleCommand(actions, input);
        require(command.movement == glm::vec2(1, -1) && command.sprint && command.jumpPressed,
            "Default input mapping failed");
        input.beginFrame();
        require(!sampleCommand(actions, input).jumpPressed, "Held jump became a second request");
        const auto unfocused = sampleCommand(actions, input, {}, false);
        require(unfocused.movement == glm::vec2(0) && !unfocused.sprint && !unfocused.jumpPressed,
            "Unfocused input was not suppressed");
        input.keyEvent(Key::S, true);
        input.keyEvent(Key::A, true);
        require(sampleCommand(actions, input).movement == glm::vec2(0), "Opposing keys did not cancel");

        // 同名动作已有自定义绑定时，注册函数不得增加W/Shift；连续调用也保持这一契约。
        PlayerBindings bindings;
        bindings.forward = "go";
        bindings.sprint = "boost";
        ActionMap custom;
        custom.createAction("go");
        custom.bindKey("go", Key::Up);
        custom.createAction("boost");
        custom.bindKey("boost", Key::Enter);
        custom.createAction(bindings.jump); // 空绑定也代表用户选择，不能自动补Space。
        registerDefaultActions(custom, bindings);
        registerDefaultActions(custom, bindings);
        InputState keys;
        keys.keyEvent(Key::W, true);
        keys.keyEvent(Key::RightShift, true);
        keys.keyEvent(Key::Space, true);
        auto mapped = sampleCommand(custom, keys, bindings);
        require(mapped.movement == glm::vec2(0) && !mapped.sprint && !mapped.jumpPressed,
            "Custom/empty bindings received default keys");
        keys.keyEvent(Key::Up, true);
        keys.keyEvent(Key::Enter, true);
        mapped = sampleCommand(custom, keys, bindings);
        require(mapped.movement.y == -1 && mapped.sprint, "Custom actions did not drive a command");

        ActionMap empty;
        expectThrow<std::out_of_range>([&] { sampleCommand(empty, keys); }, "Sampling created actions");
        require(!empty.hasAction("move_forward"), "Sampling modified actions");
        bindings.backward = bindings.forward;
        expectThrow<std::invalid_argument>([&] { registerDefaultActions(empty, bindings); },
            "Duplicate names accepted");
        require(!empty.hasAction("go"), "Invalid bindings left partial registration");
        bindings = {};
        bindings.jump.clear();
        expectThrow<std::invalid_argument>([&] { registerDefaultActions(empty, bindings); },
            "Empty action name accepted");
    }

    void testMovement()
    {
        const FlatGroundSettings settings{2, 2, 4, -10, 20, 0.5f};
        FlatGroundMovement movement(settings);
        Transform player;
        player.position.y = 0.5f;
        player.setEulerAngles({0.2f, 0.4f, 0.1f});
        const auto rotation = player.rotation();

        // 不创建Input/ActionMap，直接提交AI或回放也可产生的数据，验证真正的输入解耦。
        movement.update(player, {{0, -1}}, 0.5f);
        require(near(player.position.z, -1) && movement.grounded(), "Forward movement failed");
        auto before = player.position;
        movement.update(player, {{1, -1}}, 0.5f);
        require(near(glm::length(player.position - before), 1), "Diagonal speed exceeded movement speed");
        before = player.position;
        movement.update(player, {{1, 0}, false, true}, 0.5f);
        require(near(player.position.x - before.x, 2), "Sprint speed failed");
        before = player.position;
        movement.update(player, {{0.25f, 0}}, 0.5f);
        require(near(player.position.x - before.x, 0.25f), "Analog command strength lost");
        require(player.rotation() == rotation, "Movement changed presentation rotation");

        movement.update(player, {{0, 0}, true}, 0.1f);
        require(near(player.position.y, 0.85f) && near(movement.verticalVelocity(), 3) &&
            !movement.grounded(), "Jump integration failed");
        movement.update(player, {{0, 0}, true}, 0.1f);
        require(near(movement.verticalVelocity(), 2), "Air jump reset velocity");
        for (int i = 0; i < 100; ++i) { movement.update(player, {}, 0.1f); }
        require(movement.grounded() && player.position.y == 0.5f, "Landing failed");

        // 失焦策略只改变指令：人在空中仍下落，不把窗口焦点带入移动规则。
        Transform airborne;
        airborne.position.y = 10;
        FlatGroundMovement falling({2, 2, 4, -10, 3, 0.5f});
        ActionMap actions;
        registerDefaultActions(actions);
        InputState held;
        held.keyEvent(Key::D, true);
        falling.update(airborne, sampleCommand(actions, held, {}, false), 1);
        require(near(airborne.position.y, 7.45f) && airborne.position.x == 0 &&
            falling.verticalVelocity() == -3, "Unfocused fall or terminal speed failed");

        Transform one, many;
        one.position.y = many.position.y = 0.5f;
        FlatGroundMovement oneStep(settings), manySteps(settings);
        oneStep.update(one, {{0, 0}, true}, 0.2f);
        for (int i = 0; i < 10; ++i) { manySteps.update(many, {{0, 0}, i == 0}, 0.02f); }
        require(near(one.position.y, many.position.y) &&
            near(oneStep.verticalVelocity(), manySteps.verticalVelocity()), "Jump depends on frame partition");

        // 小步/大步跨过终端速度分界时，积分也应相符。
        one.position.y = many.position.y = 100;
        FlatGroundMovement fallOne({2, 2, 4, -10, 3, 0.5f}), fallMany({2, 2, 4, -10, 3, 0.5f});
        fallOne.update(one, {}, 1);
        for (int i = 0; i < 10; ++i) { fallMany.update(many, {}, 0.1f); }
        require(near(one.position.y, many.position.y), "Terminal-speed partition mismatch");

        // 把真实输入适配与移动规则串起来：持续按住跳跃，落地后不能再次自动起跳。
        // 第二个角色直接接收相同指令，验证非键盘来源和键盘来源有一致结果，状态彼此独立。
        Transform keyboardPlayer, scriptedPlayer;
        keyboardPlayer.position.y = scriptedPlayer.position.y = 0.5f;
        FlatGroundMovement keyboardMotion(settings), scriptedMotion(settings);
        InputState jumpHeld;
        jumpHeld.keyEvent(Key::Space, true);
        for (int frame = 0; frame < 30; ++frame)
        {
            keyboardMotion.update(keyboardPlayer, sampleCommand(actions, jumpHeld), 0.1f);
            scriptedMotion.update(scriptedPlayer, {{0, 0}, frame == 0}, 0.1f);
            require(keyboardPlayer.position == scriptedPlayer.position &&
                keyboardMotion.verticalVelocity() == scriptedMotion.verticalVelocity(),
                "Keyboard and direct command paths differ");
            if (frame >= 10) { require(keyboardMotion.grounded(), "Held jump retriggered after landing"); }
            jumpHeld.beginFrame();
        }
        keyboardMotion.update(keyboardPlayer, {{1, 0}}, 0.1f);
        require(scriptedPlayer.position.x == 0 && keyboardPlayer.position.x > 0,
            "Separate actors unexpectedly share movement");
    }

    void testValidation()
    {
        const FlatGroundSettings settings{2, 2, 4, -10, 20, 0.5f};
        FlatGroundMovement movement(settings);
        Transform player;
        player.position = {2, 0.5f, 3};
        player.setEulerAngles({0.2f, 0.3f, 0.4f});
        const auto rotation = player.rotation();
        movement.update(player, {{1, 0}, true}, 0);
        require(player.position == glm::vec3(2, 0.5f, 3) && !movement.grounded() &&
            movement.verticalVelocity() == 0, "Zero dt changed state");
        movement.update(player, {{0, 0}, true}, 0.1f);
        const auto jumping = player.position;
        const auto velocity = movement.verticalVelocity();
        expectThrow<std::invalid_argument>([&] { movement.update(player, {}, -1); }, "Negative dt accepted");
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        expectThrow<std::invalid_argument>([&] { movement.update(player, {}, nan); }, "NaN dt accepted");
        expectThrow<std::invalid_argument>([&] { movement.update(player, {{nan, 0}}, 1); }, "NaN command accepted");
        require(player.position == jumping && movement.verticalVelocity() == velocity &&
            player.rotation() == rotation, "Invalid input partially changed state");
        movement.reset(player);
        require(player.position == glm::vec3(2, 0.5f, 3) && movement.grounded() &&
            movement.verticalVelocity() == 0 && player.rotation() == rotation, "Reset changed unrelated state");

        Transform parent;
        player.setParent(&parent);
        expectThrow<std::invalid_argument>([&] { movement.update(player, {}, 0.1f); }, "Parented transform accepted");
        expectThrow<std::invalid_argument>([&] { movement.reset(player); }, "Parented reset accepted");
        player.setParent(nullptr);
        player.position.x = nan;
        expectThrow<std::invalid_argument>([&] { movement.reset(player); }, "Nonfinite reset accepted");

        const auto maximum = std::numeric_limits<float>::max();
        FlatGroundSettings fast;
        fast.moveSpeed = maximum;
        FlatGroundMovement extreme(fast);
        Transform overflow;
        expectThrow<std::invalid_argument>([&] { extreme.update(overflow, {{1, 0}}, 2); }, "Overflow accepted");
        require(overflow.position == glm::vec3(0) && extreme.verticalVelocity() == 0 &&
            !extreme.grounded(), "Overflow left partial state");

        // 大输入只限幅而不是溢出归一化；不依赖键盘的指令入口必须处理外部数值。
        FlatGroundMovement bounded;
        bounded.update(overflow, {{maximum, maximum}}, 1);
        require(near(glm::length(overflow.position), 3), "Large finite command normalization failed");
        FlatGroundSettings invalid;
        invalid.gravity = 0;
        expectThrow<std::invalid_argument>([&] { FlatGroundMovement bad(invalid); }, "Nonnegative gravity accepted");
        invalid = {};
        invalid.groundHeight = nan;
        expectThrow<std::invalid_argument>([&] { FlatGroundMovement bad(invalid); }, "NaN settings accepted");
    }
}

int main()
{
    try
    {
        testInput();
        testMovement();
        testValidation();
        std::cout << "Player example: explicit bindings, command-driven motion and validation passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
