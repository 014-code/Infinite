#include "ui/font/Utf8.h"
#include "ui/font/FontFile.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void require(bool condition, const char *message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

int main(int argc, char *argv[])
{
    try
    {
        const std::vector<std::uint32_t> mixed = UiFont::decodeUtf8("A中😀");
        require(mixed.size() == 3, "UTF-8 mixed text length is incorrect");
        require(mixed[0] == 'A' && mixed[1] == 0x4E2Du && mixed[2] == 0x1F600u,
            "UTF-8 mixed text was decoded incorrectly");

        std::string invalidText = "A";
        invalidText.push_back(static_cast<char>(0xF0));
        invalidText.push_back('(');
        invalidText.push_back(static_cast<char>(0x8C));
        invalidText.push_back('(');
        invalidText.push_back('B');
        const std::vector<std::uint32_t> invalid = UiFont::decodeUtf8(invalidText);
        require(invalid.size() == 6, "Invalid UTF-8 should preserve later bytes");
        require(invalid[0] == 'A' && invalid[1] == UiFont::ReplacementCharacter &&
            invalid[2] == '(' && invalid[3] == UiFont::ReplacementCharacter &&
            invalid[4] == '(' && invalid[5] == 'B',
            "Invalid UTF-8 replacement behavior is incorrect");

        const std::vector<std::uint32_t> newline = UiFont::decodeUtf8("上\n下");
        require(newline.size() == 3 && newline[1] == '\n',
            "UTF-8 newline was not preserved");

        // 传入字体路径时额外验证真实TTF/OTF资源；默认CTest不依赖本机系统字体。
        if (argc == 2)
        {
            UiFont::FontFile font{std::filesystem::path(argv[1])};
            require(font.hasGlyph('A'), "Font file does not contain the ASCII A glyph");
            const UiFont::FontMetrics metrics = font.metrics(64.0f);
            require(metrics.lineHeight > 0.0f && metrics.ascent > 0.0f,
                "Font metrics are invalid");
            const UiFont::RasterizedGlyph glyph = font.rasterize('A', 64.0f);
            require(glyph.width > 0 && glyph.height > 0 && !glyph.pixels.empty(),
                "Font glyph rasterization returned no pixels");
        }

        std::cout << "UI font UTF-8 test passed\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
