#include "ui/UiButton.h"
#include "ui/UiCanvas.h"
#include "ui/UiLabel.h"
#include "ui/UiPanel.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char *message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

int main()
{
    try
    {
        UiCanvas canvas;
        auto &panel = canvas.create<UiPanel>();
        panel.setRect({20.0f, 20.0f, 300.0f, 240.0f});
        auto &label = panel.createChild<UiLabel>("MENU");
        label.setRect({20.0f, 20.0f, 260.0f, 40.0f});
        label.setAlignment(UiTextAlign::Center);
        auto &button = panel.createChild<UiButton>("PLAY");
        button.setRect({50.0f, 100.0f, 200.0f, 60.0f});

        int activations = 0;
        button.setOnActivated([&activations] { ++activations; });

        InputState input;
        input.cursorEvent(120.0, 150.0);
        input.mouseButtonEvent(MouseButton::Left, true);
        canvas.processInput(input, {640.0f, 480.0f});
        require(canvas.hoveredElement() == &button, "UI mouse hit test failed");
        require(button.isPressed(), "UI button press state was not set");

        input.beginFrame();
        input.mouseButtonEvent(MouseButton::Left, false);
        canvas.processInput(input, {640.0f, 480.0f});
        require(activations == 1, "UI mouse activation failed");

        input.beginFrame();
        input.keyEvent(Key::Tab, true);
        canvas.processInput(input, {640.0f, 480.0f});
        require(canvas.focusedElement() == &button && button.isFocused(),
            "UI keyboard focus failed");

        input.beginFrame();
        input.keyEvent(Key::Enter, true);
        canvas.processInput(input, {640.0f, 480.0f});
        require(activations == 2, "UI keyboard activation failed");

        UiRenderCommandList commands;
        canvas.collectCommands(commands);
        require(commands.size() == 4, "UI command collection returned an unexpected count");
        require(commands[0].type == UiRenderCommandType::Rectangle,
            "UI panel command missing");
        require(commands[1].type == UiRenderCommandType::Text && commands[1].text == "MENU",
            "UI label command missing");
        require(commands[2].type == UiRenderCommandType::Rectangle,
            "UI button background command missing");
        require(commands[3].type == UiRenderCommandType::Text && commands[3].text == "PLAY",
            "UI button label command missing");

        std::cout << "UI system passed\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
