#include "ui/font/Utf8.h"

#include <cstddef>

namespace
{
    bool isContinuationByte(unsigned char value) noexcept
    {
        return (value & 0xC0u) == 0x80u;
    }

    bool isValidCodePoint(std::uint32_t value) noexcept
    {
        return value <= 0x10FFFFu && !(value >= 0xD800u && value <= 0xDFFFu);
    }
}

namespace UiFont
{
    std::vector<std::uint32_t> decodeUtf8(std::string_view text)
    {
        std::vector<std::uint32_t> result;
        result.reserve(text.size());

        std::size_t index = 0;
        while (index < text.size())
        {
            const unsigned char first = static_cast<unsigned char>(text[index]);
            std::uint32_t codePoint = ReplacementCharacter;
            std::size_t byteCount = 1;
            bool valid = true;

            if (first <= 0x7Fu)
            {
                codePoint = first;
            }
            else if (first >= 0xC2u && first <= 0xDFu)
            {
                byteCount = 2;
                valid = index + byteCount <= text.size() &&
                    isContinuationByte(static_cast<unsigned char>(text[index + 1]));
                if (valid)
                {
                    codePoint = (static_cast<std::uint32_t>(first & 0x1Fu) << 6) |
                        (static_cast<unsigned char>(text[index + 1]) & 0x3Fu);
                }
            }
            else if (first >= 0xE0u && first <= 0xEFu)
            {
                byteCount = 3;
                valid = index + byteCount <= text.size();
                if (valid)
                {
                    const unsigned char second = static_cast<unsigned char>(text[index + 1]);
                    const unsigned char third = static_cast<unsigned char>(text[index + 2]);
                    valid = isContinuationByte(second) && isContinuationByte(third);
                    // 防止E0过长编码和ED编码进入UTF-16代理区。
                    valid = valid && !(first == 0xE0u && second < 0xA0u);
                    valid = valid && !(first == 0xEDu && second >= 0xA0u);
                    if (valid)
                    {
                        codePoint = (static_cast<std::uint32_t>(first & 0x0Fu) << 12) |
                            (static_cast<std::uint32_t>(second & 0x3Fu) << 6) |
                            (third & 0x3Fu);
                    }
                }
            }
            else if (first >= 0xF0u && first <= 0xF4u)
            {
                byteCount = 4;
                valid = index + byteCount <= text.size();
                if (valid)
                {
                    const unsigned char second = static_cast<unsigned char>(text[index + 1]);
                    const unsigned char third = static_cast<unsigned char>(text[index + 2]);
                    const unsigned char fourth = static_cast<unsigned char>(text[index + 3]);
                    valid = isContinuationByte(second) && isContinuationByte(third) &&
                        isContinuationByte(fourth);
                    // 防止F0过长编码和F4超出Unicode最大码点。
                    valid = valid && !(first == 0xF0u && second < 0x90u);
                    valid = valid && !(first == 0xF4u && second > 0x8Fu);
                    if (valid)
                    {
                        codePoint = (static_cast<std::uint32_t>(first & 0x07u) << 18) |
                            (static_cast<std::uint32_t>(second & 0x3Fu) << 12) |
                            (static_cast<std::uint32_t>(third & 0x3Fu) << 6) |
                            (fourth & 0x3Fu);
                    }
                }
            }
            else
            {
                // C0/C1、F5以上和孤立的延续字节都不是合法的UTF-8起始字节。
                valid = false;
            }

            if (!valid || !isValidCodePoint(codePoint))
            {
                codePoint = ReplacementCharacter;
                // 只跳过当前坏字节，让后面的ASCII内容仍能显示。
                byteCount = 1;
            }

            result.push_back(codePoint);
            index += byteCount;
        }
        return result;
    }
}
