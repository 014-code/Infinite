#include "ui/font/FontCollection.h"

#include <stdexcept>

namespace UiFont
{
    FontCollection::FontCollection(std::shared_ptr<FontFile> primary)
    {
        if (!primary)
        {
            throw std::invalid_argument("FontCollection requires a primary font");
        }
        fonts_.push_back(std::move(primary));
    }

    void FontCollection::addFallback(std::shared_ptr<FontFile> fallback)
    {
        if (!fallback)
        {
            throw std::invalid_argument("FontCollection fallback must not be null");
        }
        fonts_.push_back(std::move(fallback));
    }

    const FontFile *FontCollection::fontFor(std::uint32_t codePoint) const noexcept
    {
        for (const auto &font : fonts_)
        {
            if (font->hasGlyph(codePoint))
            {
                return font.get();
            }
        }
        // 缺字时由主字体的问号或替换字形负责显示，不让布局直接崩溃。
        return fonts_.front().get();
    }
}
