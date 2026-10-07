#include "UiButton.h"

#include <utility>

void UiButton::activate()
{
    if (onActivated_)
    {
        onActivated_();
    }
}

void UiButton::appendCommands(UiRenderCommandList &commands, const UiRect &global) const
{
    UiRenderCommand background;
    background.type = UiRenderCommandType::Rectangle;
    background.rect = global;
    background.color = pressed_ ? pressedColor_ :
        (hovered_ ? hoveredColor_ : (focused_ ? focusedColor_ : normalColor_));
    commands.push_back(std::move(background));

    UiRenderCommand label;
    label.type = UiRenderCommandType::Text;
    label.rect = global;
    label.color = {0.96f, 0.98f, 1.0f, 1.0f};
    label.text = text_;
    label.textScale = 2.0f;
    label.textAlign = UiTextAlign::Center;
    commands.push_back(std::move(label));
}
