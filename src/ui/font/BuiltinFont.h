#pragma once

#include <array>
#include <string_view>

namespace UiFont
{
    // 旧版无资源回退字体，保留在独立模块中，避免UiRenderer继续堆积字符表。
    std::array<std::string_view, 7> builtinGlyph(char character);
}
