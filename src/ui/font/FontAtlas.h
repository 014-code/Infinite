#pragma once

#include "ui/font/FontCollection.h"

#include <GL/glew.h>
#include <glm/vec2.hpp>

#include <cstdint>
#include <unordered_map>

namespace UiFont
{
    // FontAtlas只负责把CPU字形放入OpenGL单通道纹理；文字换行和对齐属于TextLayout。
    class FontAtlas final
    {
    public:
        struct Glyph
        {
            int width = 0;
            int height = 0;
            int bearingX = 0;
            int bearingY = 0;
            float advance = 0.0f;
            glm::vec2 uvMin{0.0f};
            glm::vec2 uvMax{0.0f};
            bool drawable = false;
        };

        explicit FontAtlas(const FontCollection &fonts, int pixelHeight = 64, int textureSize = 2048);
        ~FontAtlas();

        FontAtlas(const FontAtlas &) = delete;
        FontAtlas &operator=(const FontAtlas &) = delete;

        const Glyph &glyph(std::uint32_t codePoint);
        float pixelHeight() const noexcept { return static_cast<float>(pixelHeight_); }
        void bind(GLuint textureUnit = 0) const;

    private:
        Glyph uploadGlyph(std::uint32_t codePoint);

        const FontCollection &fonts_;
        int pixelHeight_ = 64;
        int textureSize_ = 2048;
        GLuint texture_ = 0;
        int nextX_ = 1;
        int nextY_ = 1;
        int rowHeight_ = 0;
        std::unordered_map<std::uint32_t, Glyph> glyphs_;
    };
}
