#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <vector>

// UI使用窗口逻辑像素作为布局单位，原点在窗口左上角。
// Renderer会在高DPI窗口中把逻辑像素换算为实际帧缓冲像素。
struct UiRect
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool contains(const glm::vec2 &point) const noexcept
    {
        return point.x >= x && point.y >= y &&
            point.x <= x + width && point.y <= y + height;
    }
};

enum class UiTextAlign
{
    Left,
    Center,
    Right
};

enum class UiRenderCommandType
{
    Rectangle,
    Text
};

// UiElement只生成简单的绘制命令，不直接调用OpenGL。
// 这样UI布局和命中测试可以在无窗口测试中验证，GPU绘制集中在UiRenderer。
struct UiRenderCommand
{
    UiRenderCommandType type = UiRenderCommandType::Rectangle;
    UiRect rect;
    glm::vec4 color{1.0f};
    std::string text;
    float textScale = 1.0f;
    UiTextAlign textAlign = UiTextAlign::Left;
};

using UiRenderCommandList = std::vector<UiRenderCommand>;
