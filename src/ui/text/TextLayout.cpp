#include "ui/text/TextLayout.h"

#include "ui/font/Utf8.h"

#include <algorithm>

namespace UiText
{
    TextLayoutResult layout(std::string_view text, const UiFont::FontCollection &fonts,
        float pixelHeight, float letterSpacing)
    {
        TextLayoutResult result;
        result.metrics = fonts.primary().metrics(pixelHeight);
        const auto codePoints = UiFont::decodeUtf8(text);
        result.lines.emplace_back();

        for (const std::uint32_t codePoint : codePoints)
        {
            if (codePoint == '\n')
            {
                result.lines.emplace_back();
                continue;
            }

            TextLine &line = result.lines.back();
            const UiFont::FontFile *font = fonts.fontFor(codePoint);
            const std::uint32_t measuredCodePoint = font->hasGlyph(codePoint) ? codePoint : '?';
            const float advance = font->advance(measuredCodePoint, pixelHeight);
            if (!line.codePoints.empty())
            {
                line.width += letterSpacing;
            }
            line.width += advance;
            line.codePoints.push_back(codePoint);
        }

        for (const TextLine &line : result.lines)
        {
            result.width = std::max(result.width, line.width);
        }
        result.height = result.metrics.lineHeight * static_cast<float>(result.lines.size());
        return result;
    }
}
