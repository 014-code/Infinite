#include "TestSupport.h"
#include "input/ActionMap.h"

#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        ActionMap actions;
        actions.createAction("move_forward");
        actions.createAction("select");
        actions.createAction("empty");
        actions.createAction("move_forward");
        actions.bindKey("move_forward", Key::W);
        actions.bindKey("move_forward", Key::Up);
        actions.bindKey("move_forward", Key::W); // 重复绑定应保持幂等。
        actions.bindMouseButton("select", MouseButton::Left);
        require(actions.hasAction("move_forward") && actions.hasAction("select"),
            "Action registration failed");

        InputState state;
        require(!actions.isDown("empty", state), "Empty action was unexpectedly active");
        state.keyEvent(Key::Up, true);
        require(actions.isDown("move_forward", state) && actions.wasPressed("move_forward", state),
            "Keyboard action OR mapping failed");
        state.beginFrame();
        require(actions.isDown("move_forward", state) && !actions.wasPressed("move_forward", state),
            "Held action produced a repeated press");
        state.keyEvent(Key::Up, false);
        require(actions.wasReleased("move_forward", state) && !actions.isDown("move_forward", state),
            "Keyboard action release failed");

        state.beginFrame();
        state.mouseButtonEvent(MouseButton::Left, true);
        require(actions.isDown("select", state) && actions.wasPressed("select", state),
            "Mouse action press failed");
        state.focusLost();
        require(actions.wasReleased("select", state) && !actions.isDown("select", state),
            "Focus loss did not release mouse action");

        expectThrow<std::out_of_range>([&] { actions.bindKey("missing", Key::A); },
            "Binding an unknown action was accepted");
        expectThrow<std::out_of_range>([&] { actions.isDown("missing", state); },
            "Querying an unknown action was accepted");
        expectThrow<std::invalid_argument>([&] { actions.createAction(""); },
            "Empty action name was accepted");
        expectThrow<std::invalid_argument>([&] { actions.bindKey("empty", Key::Count); },
            "Invalid key was accepted");
        expectThrow<std::invalid_argument>([&] { actions.bindMouseButton("empty", MouseButton::Count); },
            "Invalid mouse button was accepted");
        actions.clearBindings("move_forward");
        require(!actions.isDown("move_forward", state), "Clearing action bindings failed");
        require(actions.removeAction("empty") && !actions.hasAction("empty") &&
            !actions.removeAction("empty"), "Action removal semantics failed");

        std::cout << "Input action mapping passed: keyboard, mouse, edges and validation\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Input action mapping failed: " << error.what() << '\n';
        return 1;
    }
}
