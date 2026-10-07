#include "UiRenderer.h"

#include "UiCanvas.h"
#include "graphics/resources/Shader.h"
#include "ui/font/BuiltinFont.h"
#include "ui/font/FontAtlas.h"
#include "ui/font/FontFile.h"
#include "ui/text/TextLayout.h"

#include <GL/glew.h>

#include <cctype>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::string_view kVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 aPosition;
        layout (location = 1) in vec4 aColor;
        layout (location = 2) in vec2 aUv;
        layout (location = 3) in float aTextured;
        uniform vec3 viewportSize;
        out vec4 vertexColor;
        out vec2 textureCoordinate;
        out float textured;
        void main()
        {
            vec2 ndc = vec2(aPosition.x / viewportSize.x * 2.0 - 1.0,
                1.0 - aPosition.y / viewportSize.y * 2.0);
            gl_Position = vec4(ndc, 0.0, 1.0);
            vertexColor = aColor;
            textureCoordinate = aUv;
            textured = aTextured;
        }
    )";

    constexpr std::string_view kFragmentShader = R"(
        #version 330 core
        in vec4 vertexColor;
        in vec2 textureCoordinate;
        in float textured;
        uniform sampler2D glyphAtlas;
        out vec4 fragmentColor;
        void main()
        {
            if (textured > 0.5)
            {
                float coverage = texture(glyphAtlas, textureCoordinate).r;
                fragmentColor = vec4(vertexColor.rgb, vertexColor.a * coverage);
            }
            else
            {
                fragmentColor = vertexColor;
            }
        }
    )";

    void addQuad(std::vector<UiRenderer::Vertex> &vertices, float left, float top,
        float right, float bottom, const glm::vec4 &color,
        const glm::vec2 &uvMin = {0.0f, 0.0f}, const glm::vec2 &uvMax = {0.0f, 0.0f},
        float textured = 0.0f)
    {
        // 顶点位置使用左上角坐标；纹理坐标使用OpenGL的左下角V轴。
        const UiRenderer::Vertex a{{left, top}, color, {uvMin.x, uvMax.y}, textured};
        const UiRenderer::Vertex b{{right, top}, color, {uvMax.x, uvMax.y}, textured};
        const UiRenderer::Vertex c{{right, bottom}, color, {uvMax.x, uvMin.y}, textured};
        const UiRenderer::Vertex d{{left, bottom}, color, {uvMin.x, uvMin.y}, textured};
        vertices.insert(vertices.end(), {a, b, c, a, c, d});
    }
}

UiRenderer::UiRenderer(const std::filesystem::path &fontPath)
    : shader_(std::make_unique<Shader>(Shader::fromSource(
        std::string(kVertexShader), std::string(kFragmentShader), "built-in UI shader")))
{
    if (!fontPath.empty())
    {
        // FontFile只在这里创建，UiLabel和UiCanvas不依赖第三方字体库或OpenGL资源。
        auto primaryFont = std::make_shared<UiFont::FontFile>(fontPath);
        fonts_ = std::make_shared<UiFont::FontCollection>(std::move(primaryFont));
        fontAtlas_ = std::make_unique<UiFont::FontAtlas>(*fonts_);
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    if (vao_ == 0 || vbo_ == 0)
    {
        throw std::runtime_error("Failed to create UI vertex buffers");
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void *>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void *>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void *>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void *>(offsetof(Vertex, textured)));
    glBindVertexArray(0);
}

UiRenderer::~UiRenderer()
{
    if (vbo_ != 0) { glDeleteBuffers(1, &vbo_); }
    if (vao_ != 0) { glDeleteVertexArrays(1, &vao_); }
}

void UiRenderer::appendRectangle(std::vector<Vertex> &vertices, const UiRect &rect,
    const glm::vec4 &color, float scaleX, float scaleY)
{
    addQuad(vertices, rect.x * scaleX, rect.y * scaleY,
        (rect.x + rect.width) * scaleX, (rect.y + rect.height) * scaleY, color);
}

void UiRenderer::appendText(std::vector<Vertex> &vertices, const UiRenderCommand &command,
    float scaleX, float scaleY)
{
    if (fontAtlas_)
    {
        appendFontText(vertices, command, scaleX, scaleY);
    }
    else
    {
        appendBuiltinText(vertices, command, scaleX, scaleY);
    }
}

void UiRenderer::appendBuiltinText(std::vector<Vertex> &vertices,
    const UiRenderCommand &command, float scaleX, float scaleY) const
{
    const float glyphWidth = 5.0f * command.textScale;
    const float glyphHeight = 7.0f * command.textScale;
    const float spacing = command.textScale;
    const float textWidth = command.text.empty() ? 0.0f :
        glyphWidth * static_cast<float>(command.text.size()) +
        spacing * static_cast<float>(command.text.size() - 1);
    float x = command.rect.x;
    if (command.textAlign == UiTextAlign::Center)
    {
        x += (command.rect.width - textWidth) * 0.5f;
    }
    else if (command.textAlign == UiTextAlign::Right)
    {
        x += command.rect.width - textWidth;
    }
    const float y = command.rect.y + (command.rect.height - glyphHeight) * 0.5f;

    for (std::size_t characterIndex = 0; characterIndex < command.text.size(); ++characterIndex)
    {
        const char character = static_cast<char>(std::toupper(
            static_cast<unsigned char>(command.text[characterIndex])));
        const auto rows = UiFont::builtinGlyph(character);
        for (std::size_t row = 0; row < rows.size(); ++row)
        {
            for (std::size_t column = 0; column < rows[row].size(); ++column)
            {
                if (rows[row][column] != '1')
                {
                    continue;
                }
                const float left = (x + static_cast<float>(column) * command.textScale) * scaleX;
                const float top = (y + static_cast<float>(row) * command.textScale) * scaleY;
                addQuad(vertices, left, top, left + command.textScale * scaleX,
                    top + command.textScale * scaleY, command.color);
            }
        }
        x += glyphWidth + spacing;
    }
}

void UiRenderer::appendFontText(std::vector<Vertex> &vertices,
    const UiRenderCommand &command, float scaleX, float scaleY)
{
    // 用64像素缓存字形，再按旧版5×7字形的scale换算到逻辑UI尺寸，
    // 这样升级字体后现有示例的布局不会突然放大很多倍。
    constexpr float kAtlasPixelHeight = 64.0f;
    const float logicalScale = command.textScale * 7.0f / kAtlasPixelHeight;
    // TextLayout使用字形图集的像素坐标，所以把旧版逻辑间距换算成图集像素。
    // 这样旧版scale=2时仍然保持约2个逻辑像素的字符间距。
    const float atlasLetterSpacing = kAtlasPixelHeight / 7.0f;
    const UiText::TextLayoutResult layout = UiText::layout(command.text, *fonts_,
        kAtlasPixelHeight, atlasLetterSpacing);
    const float totalHeight = layout.height * logicalScale;
    const float firstBaseline = command.rect.y +
        (command.rect.height - totalHeight) * 0.5f + layout.metrics.ascent * logicalScale;

    for (std::size_t lineIndex = 0; lineIndex < layout.lines.size(); ++lineIndex)
    {
        const UiText::TextLine &line = layout.lines[lineIndex];
        float x = command.rect.x;
        const float lineWidth = line.width * logicalScale;
        if (command.textAlign == UiTextAlign::Center)
        {
            x += (command.rect.width - lineWidth) * 0.5f;
        }
        else if (command.textAlign == UiTextAlign::Right)
        {
            x += command.rect.width - lineWidth;
        }

        const float baseline = firstBaseline + static_cast<float>(lineIndex) *
            layout.metrics.lineHeight * logicalScale;
        for (std::size_t characterIndex = 0; characterIndex < line.codePoints.size(); ++characterIndex)
        {
            const std::uint32_t codePoint = line.codePoints[characterIndex];
            const UiFont::FontFile *font = fonts_->fontFor(codePoint);
            const std::uint32_t measuredCodePoint = font->hasGlyph(codePoint) ? codePoint : '?';
            const UiFont::FontAtlas::Glyph &glyph = fontAtlas_->glyph(codePoint);
            const float left = (x + static_cast<float>(glyph.bearingX) * logicalScale) * scaleX;
            const float top = (baseline - static_cast<float>(glyph.bearingY) * logicalScale) * scaleY;
            const float right = left + static_cast<float>(glyph.width) * logicalScale * scaleX;
            const float bottom = top + static_cast<float>(glyph.height) * logicalScale * scaleY;
            if (glyph.drawable && measuredCodePoint != '\n')
            {
                addQuad(vertices, left, top, right, bottom, command.color,
                    glyph.uvMin, glyph.uvMax, 1.0f);
            }
            x += glyph.advance * logicalScale;
            if (characterIndex + 1 < line.codePoints.size())
            {
                x += atlasLetterSpacing * logicalScale;
            }
        }
    }
}

void UiRenderer::render(const UiCanvas &canvas, const glm::ivec2 &logicalSize,
    const glm::ivec2 &framebufferSize)
{
    if (logicalSize.x <= 0 || logicalSize.y <= 0 || framebufferSize.x <= 0 || framebufferSize.y <= 0)
    {
        return;
    }

    UiRenderCommandList commands;
    canvas.collectCommands(commands);
    if (commands.empty())
    {
        return;
    }

    std::vector<Vertex> vertices;
    const float scaleX = static_cast<float>(framebufferSize.x) / logicalSize.x;
    const float scaleY = static_cast<float>(framebufferSize.y) / logicalSize.y;
    for (const auto &command : commands)
    {
        if (command.type == UiRenderCommandType::Rectangle)
        {
            appendRectangle(vertices, command.rect, command.color, scaleX, scaleY);
        }
        else
        {
            appendText(vertices, command, scaleX, scaleY);
        }
    }
    if (vertices.empty())
    {
        return;
    }

    GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
    GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTexture = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    shader_->use();
    shader_->setVec3("viewportSize", {static_cast<float>(framebufferSize.x),
        static_cast<float>(framebufferSize.y), 0.0f});
    shader_->setInt("glyphAtlas", 0);
    if (fontAtlas_)
    {
        fontAtlas_->bind(0);
    }
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));

    if (wasDepthTestEnabled) { glEnable(GL_DEPTH_TEST); } else { glDisable(GL_DEPTH_TEST); }
    if (wasBlendEnabled) { glEnable(GL_BLEND); } else { glDisable(GL_BLEND); }
    if (wasCullEnabled) { glEnable(GL_CULL_FACE); } else { glDisable(GL_CULL_FACE); }
    glUseProgram(static_cast<GLuint>(previousProgram));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
}
