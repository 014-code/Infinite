#pragma once

#include "UiElement.h"

// 面板只负责绘制背景并提供子元素的布局坐标系。
class UiPanel final : public UiElement
{
public:
    explicit UiPanel(const glm::vec4 &color = {0.08f, 0.10f, 0.16f, 0.94f})
        : color_(color)
    {
    }

    void setColor(const glm::vec4 &color) noexcept { color_ = color; }
    const glm::vec4 &color() const noexcept { return color_; }

protected:
    void appendCommands(UiRenderCommandList &commands, const UiRect &global) const override
    {
        UiRenderCommand command;
        command.type = UiRenderCommandType::Rectangle;
        command.rect = global;
        command.color = color_;
        commands.push_back(std::move(command));
    }

private:
    glm::vec4 color_;
};
