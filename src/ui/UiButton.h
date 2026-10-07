#pragma once

#include "UiElement.h"

#include <functional>
#include <string>

// 按钮是第一阶段唯一的交互控件。它支持鼠标命中、键盘焦点和Enter/Space激活。
class UiButton final : public UiElement
{
public:
    using ActivateCallback = std::function<void()>;

    explicit UiButton(std::string text = {})
        : text_(std::move(text))
    {
    }

    void setText(std::string text) { text_ = std::move(text); }
    const std::string &text() const noexcept { return text_; }

    void setOnActivated(ActivateCallback callback) { onActivated_ = std::move(callback); }

    void setColors(const glm::vec4 &normal, const glm::vec4 &hovered,
        const glm::vec4 &pressed, const glm::vec4 &focused) noexcept
    {
        normalColor_ = normal;
        hoveredColor_ = hovered;
        pressedColor_ = pressed;
        focusedColor_ = focused;
    }

    bool isHovered() const noexcept { return hovered_; }
    bool isPressed() const noexcept { return pressed_; }
    bool isFocused() const noexcept { return focused_; }

    bool isFocusable() const noexcept override { return true; }
    void setHovered(bool hovered) noexcept override { hovered_ = hovered; }
    void setPressed(bool pressed) noexcept override { pressed_ = pressed; }
    void setFocused(bool focused) noexcept override { focused_ = focused; }
    void activate() override;

protected:
    bool isInteractive() const noexcept override { return true; }
    void appendCommands(UiRenderCommandList &commands, const UiRect &global) const override;

private:
    std::string text_;
    ActivateCallback onActivated_;
    glm::vec4 normalColor_{0.16f, 0.20f, 0.30f, 1.0f};
    glm::vec4 hoveredColor_{0.23f, 0.35f, 0.54f, 1.0f};
    glm::vec4 pressedColor_{0.10f, 0.14f, 0.22f, 1.0f};
    glm::vec4 focusedColor_{0.26f, 0.42f, 0.65f, 1.0f};
    bool hovered_ = false;
    bool pressed_ = false;
    bool focused_ = false;
};
