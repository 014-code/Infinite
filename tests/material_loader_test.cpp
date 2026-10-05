#include "TestSupport.h"
#include "assets/MaterialLoader.h"

#include <fstream>
#include <iostream>

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected output directory");
        const auto directory = std::filesystem::path(argv[1]) / std::filesystem::u8path(u8"材质 空格");
        std::filesystem::create_directories(directory);
        const auto path = directory / "test.material";
        const std::string header =
            "INFINITE_MATERIAL 1\nVERTEX_SHADER \"../shaders/a.vert\"\n"
            "FRAGMENT_SHADER \"片段 shader.frag\"\nTEXTURE NONE\n"
            "BASE_COLOR 1 0.5 0.2 0.7\nRENDER_MODE AlphaBlend\nCULL_MODE None\n";
        const auto write = [&](const std::string &text)
        {
            std::ofstream file(path);
            file << text;
            file.close();
            require(bool(file), "Could not write material fixture");
        };
        write(header + "END_MATERIAL\n");
        const auto data = MaterialLoader::load(path);
        require(!data.hasTexture && data.renderMode == RenderMode::AlphaBlend &&
            data.cullMode == CullMode::None && data.baseColor.a == 0.7f, "Material fields lost");
        require(data.fragmentShaderPath == std::filesystem::absolute(
            directory / std::filesystem::u8path(u8"片段 shader.frag")).lexically_normal(),
            "UTF-8 relative path was not resolved against material directory");
        // 验证解析器不依赖Shader文件存在，更不需要图形上下文。
        for (const auto &text : {header, header + "CULL_MODE Back\nEND_MATERIAL",
            header + "UNKNOWN value\nEND_MATERIAL", header + "END_MATERIAL extra",
            std::string("INFINITE_MATERIAL 99")})
        {
            write(text);
            expectThrow<std::runtime_error>([&] { MaterialLoader::load(path); }, "Invalid format accepted");
        }
        auto invalidColor = header;
        invalidColor.replace(invalidColor.find("0.7"), 3, "1.7");
        write(invalidColor + "END_MATERIAL");
        expectThrow<std::runtime_error>([&] { MaterialLoader::load(path); }, "Invalid alpha accepted");
        auto invalidMode = header;
        invalidMode.replace(invalidMode.find("AlphaBlend"), 10, "BadMode");
        write(invalidMode + "END_MATERIAL");
        expectThrow<std::runtime_error>([&] { MaterialLoader::load(path); }, "Invalid mode accepted");
        std::cout << "Material parsing and validation passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
