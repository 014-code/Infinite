#pragma once

#include "ui/font/FontCollection.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace UiText
{
    struct TextLine
    {
        std::vector<std::uint32_t> codePoints;
        float width = 0.0f;
    };

    struct TextLayoutResult
    {
        std::vector<TextLine> lines;
        UiFont::FontMetrics metrics;
        float width = 0.0f;
        float height = 0.0f;
    };

    // 第一阶段只做明确换行和基础字距计算，不宣称支持复杂脚本 shaping。
    // 以后接入HarfBuzz时，UiRenderer仍消费TextLayoutResult，替换范围会被限制在本模块。
    TextLayoutResult layout(std::string_view text, const UiFont::FontCollection &fonts,
        float pixelHeight, float letterSpacing = 0.0f);
}
