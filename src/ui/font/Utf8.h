#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace UiFont
{
    // 非法UTF-8使用Unicode替换字符，避免坏输入让整段UI文字无法渲染。
    constexpr std::uint32_t ReplacementCharacter = 0xFFFD;

    // 将UTF-8字节序列解码为Unicode码点。该函数不依赖OpenGL，便于单元测试。
    std::vector<std::uint32_t> decodeUtf8(std::string_view text);
}
