#include "ui/font/FontFile.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr std::size_t kMaximumFontBytes = 64u * 1024u * 1024u;

    std::vector<unsigned char> readFontFile(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            throw std::runtime_error("Failed to open font file: " + path.u8string());
        }

        const std::streampos end = file.tellg();
        if (end <= 0 || static_cast<std::uintmax_t>(end) > kMaximumFontBytes)
        {
            throw std::runtime_error("Font file is empty or exceeds the 64MB limit: " + path.u8string());
        }

        const std::size_t size = static_cast<std::size_t>(end);
        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> data(size);
        if (!file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size())))
        {
            throw std::runtime_error("Failed to read font file: " + path.u8string());
        }
        return data;
    }

    float checkedPixelHeight(float pixelHeight)
    {
        if (!std::isfinite(pixelHeight) || pixelHeight <= 0.0f || pixelHeight > 512.0f)
        {
            throw std::invalid_argument("Font pixel height must be in (0, 512]");
        }
        return pixelHeight;
    }
}

namespace UiFont
{
    struct FontFile::Impl
    {
        std::vector<unsigned char> data;
        stbtt_fontinfo info{};
        int fontOffset = 0;
    };

    FontFile::FontFile(const std::filesystem::path &path)
        : path_(path), impl_(std::make_unique<Impl>())
    {
        impl_->data = readFontFile(path);
        // stb_truetype支持TTC的字体偏移查询，这里第一阶段使用文件中的第一个face。
        impl_->fontOffset = stbtt_GetFontOffsetForIndex(impl_->data.data(), 0);
        if (impl_->fontOffset < 0 || !stbtt_InitFont(&impl_->info, impl_->data.data(), impl_->fontOffset))
        {
            throw std::runtime_error("Unsupported or corrupt font file: " + path.u8string());
        }
    }

    FontFile::~FontFile() = default;

    FontFile::FontFile(FontFile &&other) noexcept
        : path_(std::move(other.path_)), impl_(std::move(other.impl_))
    {
    }

    FontFile &FontFile::operator=(FontFile &&other) noexcept
    {
        if (this != &other)
        {
            path_ = std::move(other.path_);
            impl_ = std::move(other.impl_);
        }
        return *this;
    }

    bool FontFile::hasGlyph(std::uint32_t codePoint) const noexcept
    {
        return impl_ && stbtt_FindGlyphIndex(&impl_->info, static_cast<int>(codePoint)) != 0;
    }

    float FontFile::advance(std::uint32_t codePoint, float pixelHeight) const
    {
        if (!impl_)
        {
            throw std::logic_error("Cannot query a moved-from FontFile");
        }
        const float scale = stbtt_ScaleForPixelHeight(&impl_->info, checkedPixelHeight(pixelHeight));
        int advanceWidth = 0;
        int leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(&impl_->info, static_cast<int>(codePoint),
            &advanceWidth, &leftSideBearing);
        return static_cast<float>(advanceWidth) * scale;
    }

    FontMetrics FontFile::metrics(float pixelHeight) const
    {
        if (!impl_)
        {
            throw std::logic_error("Cannot query a moved-from FontFile");
        }
        const float scale = stbtt_ScaleForPixelHeight(&impl_->info, checkedPixelHeight(pixelHeight));
        int ascent = 0;
        int descent = 0;
        int lineGap = 0;
        stbtt_GetFontVMetrics(&impl_->info, &ascent, &descent, &lineGap);

        FontMetrics result;
        result.ascent = static_cast<float>(ascent) * scale;
        result.descent = static_cast<float>(descent) * scale;
        result.lineGap = static_cast<float>(lineGap) * scale;
        result.lineHeight = result.ascent - result.descent + result.lineGap;
        return result;
    }

    RasterizedGlyph FontFile::rasterize(std::uint32_t codePoint, float pixelHeight) const
    {
        if (!impl_)
        {
            throw std::logic_error("Cannot rasterize from a moved-from FontFile");
        }
        const float scale = stbtt_ScaleForPixelHeight(&impl_->info, checkedPixelHeight(pixelHeight));

        RasterizedGlyph result;
        int advanceWidth = 0;
        int leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(&impl_->info, static_cast<int>(codePoint),
            &advanceWidth, &leftSideBearing);
        result.advance = static_cast<float>(advanceWidth) * scale;

        int width = 0;
        int height = 0;
        int offsetX = 0;
        int offsetY = 0;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&impl_->info, 0.0f, scale,
            static_cast<int>(codePoint), &width, &height, &offsetX, &offsetY);
        result.width = width;
        result.height = height;
        result.bearingX = offsetX;
        result.bearingY = -offsetY;
        if (bitmap && width > 0 && height > 0)
        {
            result.pixels.assign(bitmap, bitmap + static_cast<std::size_t>(width) *
                static_cast<std::size_t>(height));
        }
        if (bitmap)
        {
            stbtt_FreeBitmap(bitmap, nullptr);
        }
        return result;
    }
}
