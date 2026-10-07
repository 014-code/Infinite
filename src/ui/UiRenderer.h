#pragma once

#include "UiTypes.h"

#include <glm/vec2.hpp>

#include <filesystem>
#include <memory>
#include <vector>

class UiCanvas;
class Shader;
namespace UiFont
{
    class FontCollection;
    class FontAtlas;
}

// UiRenderer把Canvas生成的矩形/文字命令批量转换为一个动态顶点缓冲。
// 没有配置外部字体时使用内置5×7字形；配置字体后使用FontCollection、TextLayout和FontAtlas。
class UiRenderer final
{
public:
    struct Vertex
    {
        glm::vec2 position;
        glm::vec4 color;
        glm::vec2 uv;
        float textured = 0.0f;
    };

    // fontPath为空时保持旧行为；非空时加载TTF/OTF并启用UTF-8文字渲染。
    explicit UiRenderer(const std::filesystem::path &fontPath = {});
    ~UiRenderer();

    UiRenderer(const UiRenderer &) = delete;
    UiRenderer &operator=(const UiRenderer &) = delete;

    // logicalSize是UI布局坐标，framebufferSize是实际OpenGL视口尺寸。
    void render(const UiCanvas &canvas, const glm::ivec2 &logicalSize,
        const glm::ivec2 &framebufferSize);

private:
    static void appendRectangle(std::vector<Vertex> &vertices, const UiRect &rect,
        const glm::vec4 &color, float scaleX, float scaleY);
    void appendText(std::vector<Vertex> &vertices, const UiRenderCommand &command,
        float scaleX, float scaleY);
    void appendBuiltinText(std::vector<Vertex> &vertices, const UiRenderCommand &command,
        float scaleX, float scaleY) const;
    void appendFontText(std::vector<Vertex> &vertices, const UiRenderCommand &command,
        float scaleX, float scaleY);

    std::unique_ptr<Shader> shader_;
    std::shared_ptr<UiFont::FontCollection> fonts_;
    std::unique_ptr<UiFont::FontAtlas> fontAtlas_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
};
