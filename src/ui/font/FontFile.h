#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace UiFont
{
    struct FontMetrics
    {
        float ascent = 0.0f;
        float descent = 0.0f;
        float lineGap = 0.0f;
        float lineHeight = 0.0f;
    };

    struct RasterizedGlyph
    {
        int width = 0;
        int height = 0;
        // bearingY表示从基线到字形顶部的距离，方便转换为UI的左上角坐标。
        int bearingX = 0;
        int bearingY = 0;
        float advance = 0.0f;
        std::vector<unsigned char> pixels;
    };

    // FontFile是第一阶段的具体字体资源，负责读取TTF/OTF/TTC并提供CPU字形数据。
    // 这里不暴露stb_truetype类型，未来替换底层字体库不会影响UI上层接口。
    class FontFile final
    {
    public:
        explicit FontFile(const std::filesystem::path &path);
        ~FontFile();

        FontFile(const FontFile &) = delete;
        FontFile &operator=(const FontFile &) = delete;
        FontFile(FontFile &&other) noexcept;
        FontFile &operator=(FontFile &&other) noexcept;

        const std::filesystem::path &path() const noexcept { return path_; }
        bool hasGlyph(std::uint32_t codePoint) const noexcept;
        float advance(std::uint32_t codePoint, float pixelHeight) const;
        FontMetrics metrics(float pixelHeight) const;
        RasterizedGlyph rasterize(std::uint32_t codePoint, float pixelHeight) const;

    private:
        struct Impl;

        std::filesystem::path path_;
        std::unique_ptr<Impl> impl_;
    };
}
