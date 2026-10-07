#include "ui/font/FontAtlas.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr int kAtlasPadding = 1;

    class TextureUploadState
    {
    public:
        TextureUploadState()
        {
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment_);
            glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength_);
            glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows_);
            glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        }

        ~TextureUploadState()
        {
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_));
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignment_);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength_);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows_);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels_);
        }

        TextureUploadState(const TextureUploadState &) = delete;
        TextureUploadState &operator=(const TextureUploadState &) = delete;

    private:
        GLint texture_ = 0;
        GLint alignment_ = 4;
        GLint rowLength_ = 0;
        GLint skipRows_ = 0;
        GLint skipPixels_ = 0;
    };
}

namespace UiFont
{
    FontAtlas::FontAtlas(const FontCollection &fonts, int pixelHeight, int textureSize)
        : fonts_(fonts), pixelHeight_(pixelHeight), textureSize_(textureSize)
    {
        if (pixelHeight_ <= 0 || pixelHeight_ > 512 || textureSize_ <= 0)
        {
            throw std::invalid_argument("Invalid UI font atlas dimensions");
        }

        GLint maximumTextureSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
        textureSize_ = std::min(textureSize_, maximumTextureSize);
        if (textureSize_ < pixelHeight_ + kAtlasPadding * 2)
        {
            throw std::runtime_error("OpenGL texture limit is too small for the UI font atlas");
        }

        TextureUploadState state;
        glGenTextures(1, &texture_);
        if (texture_ == 0)
        {
            throw std::runtime_error("Failed to create UI font atlas texture");
        }
        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, textureSize_, textureSize_, 0,
            GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    FontAtlas::~FontAtlas()
    {
        if (texture_ != 0)
        {
            glDeleteTextures(1, &texture_);
        }
    }

    const FontAtlas::Glyph &FontAtlas::glyph(std::uint32_t codePoint)
    {
        const auto found = glyphs_.find(codePoint);
        if (found != glyphs_.end())
        {
            return found->second;
        }

        auto inserted = glyphs_.emplace(codePoint, uploadGlyph(codePoint));
        return inserted.first->second;
    }

    void FontAtlas::bind(GLuint textureUnit) const
    {
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, texture_);
    }

    FontAtlas::Glyph FontAtlas::uploadGlyph(std::uint32_t codePoint)
    {
        const FontFile *font = fonts_.fontFor(codePoint);
        const std::uint32_t sourceCodePoint = font->hasGlyph(codePoint) ? codePoint : '?';
        const RasterizedGlyph bitmap = font->rasterize(sourceCodePoint, pixelHeight());

        Glyph result;
        result.width = bitmap.width;
        result.height = bitmap.height;
        result.bearingX = bitmap.bearingX;
        result.bearingY = bitmap.bearingY;
        result.advance = bitmap.advance;
        if (bitmap.width <= 0 || bitmap.height <= 0 || bitmap.pixels.empty())
        {
            return result;
        }

        if (bitmap.width + kAtlasPadding * 2 > textureSize_ ||
            bitmap.height + kAtlasPadding * 2 > textureSize_)
        {
            throw std::runtime_error("Glyph is larger than the UI font atlas");
        }
        if (nextX_ + bitmap.width + kAtlasPadding > textureSize_)
        {
            nextX_ = kAtlasPadding;
            nextY_ += rowHeight_ + kAtlasPadding;
            rowHeight_ = 0;
        }
        if (nextY_ + bitmap.height + kAtlasPadding > textureSize_)
        {
            throw std::runtime_error("UI font atlas is full; use a larger atlas or fewer glyphs");
        }

        // stb的第一行是字形顶部，上传时翻转行以适配OpenGL的底部原点纹理坐标。
        std::vector<unsigned char> bottomUp(bitmap.pixels.size());
        for (int row = 0; row < bitmap.height; ++row)
        {
            const auto source = bitmap.pixels.begin() +
                static_cast<std::size_t>(row) * static_cast<std::size_t>(bitmap.width);
            auto destination = bottomUp.begin() + static_cast<std::size_t>(bitmap.height - 1 - row) *
                static_cast<std::size_t>(bitmap.width);
            std::copy(source, source + bitmap.width, destination);
        }

        TextureUploadState state;
        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, nextX_, nextY_, bitmap.width, bitmap.height,
            GL_RED, GL_UNSIGNED_BYTE, bottomUp.data());

        result.uvMin = {static_cast<float>(nextX_) / textureSize_,
            static_cast<float>(nextY_) / textureSize_};
        result.uvMax = {static_cast<float>(nextX_ + bitmap.width) / textureSize_,
            static_cast<float>(nextY_ + bitmap.height) / textureSize_};
        result.drawable = true;
        nextX_ += bitmap.width + kAtlasPadding;
        rowHeight_ = std::max(rowHeight_, bitmap.height);
        return result;
    }
}
