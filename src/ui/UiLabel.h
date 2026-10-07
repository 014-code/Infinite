#pragma once

#include "UiElement.h"

#include <string>

// 标签只保存文本、颜色和布局属性，不直接加载字体或调用OpenGL。
// ApplicationConfig配置uiFontPath后，Renderer会使用TTF/OTF和UTF-8文字；
// 没有配置时仍回退到内置英文5×7字形，保证旧示例可以独立运行。
class UiLabel final : public UiElement
{
public:
    explicit UiLabel(std::string text = {})
        : text_(std::move(text))
    {
    }

    void setText(std::string text) { text_ = std::move(text); }
    const std::string &text() const noexcept { return text_; }

    void setColor(const glm::vec4 &color) noexcept { color_ = color; }
    const glm::vec4 &color() const noexcept { return color_; }

    void setScale(float scale) noexcept { scale_ = scale > 0.0f ? scale : 1.0f; }
    float scale() const noexcept { return scale_; }

    void setAlignment(UiTextAlign alignment) noexcept { alignment_ = alignment; }
    UiTextAlign alignment() const noexcept { return alignment_; }

protected:
    void appendCommands(UiRenderCommandList &commands, const UiRect &global) const override
    {
        UiRenderCommand command;
        command.type = UiRenderCommandType::Text;
        command.rect = global;
        command.color = color_;
        command.text = text_;
        command.textScale = scale_;
        command.textAlign = alignment_;
        commands.push_back(std::move(command));
    }

private:
    std::string text_;
    glm::vec4 color_{0.94f, 0.96f, 1.0f, 1.0f};
    float scale_ = 2.0f;
    UiTextAlign alignment_ = UiTextAlign::Left;
};
