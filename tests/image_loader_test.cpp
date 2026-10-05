#include "TestSupport.h"
#include "graphics/resources/ImageLoader.h"

// 只在测试中生成微型图片，示例与框架都不依赖图片编码器。
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace
{
    void writeBytes(void *context, void *data, int size)
    {
        // 写入filesystem路径创建的流，以便覆盖Windows中文文件名场景。
        static_cast<std::ostream *>(context)->write(static_cast<char *>(data), size);
    }

    void writeText(const std::filesystem::path &path, const std::string &text)
    {
        std::ofstream file(path, std::ios::binary);
        file << text;
        require(static_cast<bool>(file), "Cannot write test fixture");
    }

    void expectFailure(const std::filesystem::path &path, const ImageLoadOptions &options = {})
    {
        try
        {
            ImageLoader::load(path, options);
        }
        catch (const std::runtime_error &error)
        {
            require(std::string(error.what()).find(path.u8string()) != std::string::npos,
                "Error must contain the image path");
            return;
        }
        throw std::runtime_error("Invalid image or configured limit was not rejected");
    }
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected fixture directory argument");
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);

        // 图片顶行红/绿、底行蓝/白；Alpha各不相同，可以检测丢Alpha和上下颠倒。
        const std::array<unsigned char, 16> source = {
            255, 0, 0, 128, 0, 255, 0, 255,
            0, 0, 255, 0, 255, 255, 255, 64
        };
        const auto pngPath = directory / "rgba.png";
        {
            std::ofstream file(pngPath, std::ios::binary);
            require(stbi_write_png_to_func(writeBytes, &file, 2, 2, 4, source.data(), 8) != 0,
                "Cannot generate PNG fixture");
            require(static_cast<bool>(file), "Cannot write PNG fixture");
        }

        // 所有加载都在没有窗口、没有OpenGL上下文的进程里完成。
        ImageLoadOptions topDown;
        topDown.flipVertically = false;
        const auto png = ImageLoader::load(pngPath, topDown);
        require(png.width == 2 && png.height == 2 && png.pixels.size() == source.size(),
            "Incorrect PNG dimensions");
        require(std::equal(png.pixels.begin(), png.pixels.end(), source.begin()),
            "PNG colors or alpha changed");
        const auto flipped = ImageLoader::load(pngPath);
        require(std::equal(flipped.pixels.begin(), flipped.pixels.begin() + 8, source.begin() + 8) &&
            std::equal(flipped.pixels.begin() + 8, flipped.pixels.end(), source.begin()),
            "PNG row flip is incorrect");
        require(ImageLoader::load(pngPath, topDown).pixels == png.pixels,
            "Flip option leaked into the next load");

        // Unicode字面量以转义写出，不依赖编译器源文件字符集配置。
        const auto unicodePath = directory / std::filesystem::path(L"\u7eb9\u7406.png");
        std::filesystem::copy_file(pngPath, unicodePath, std::filesystem::copy_options::overwrite_existing);
        require(ImageLoader::load(unicodePath).pixels == flipped.pixels, "Unicode path failed");

        // 灰度PNG也必须输出RGBA，RGB三个分量一致，缺失的Alpha补255。
        const unsigned char gray = 73;
        const auto grayPath = directory / "gray.png";
        {
            std::ofstream file(grayPath, std::ios::binary);
            require(stbi_write_png_to_func(writeBytes, &file, 1, 1, 1, &gray, 1) != 0,
                "Cannot generate grayscale PNG");
        }
        require(ImageLoader::load(grayPath).pixels == std::vector<unsigned char>({73, 73, 73, 255}),
            "Grayscale expansion failed");

        // 同时验证三种常用格式；JPEG为有损压缩，因此使用纯色和少量容差。
        const std::array<unsigned char, 3> rgb = {30, 100, 200};
        for (const std::string extension : {"bmp", "tga", "jpg"})
        {
            const auto path = directory / ("rgb." + extension);
            {
                std::ofstream file(path, std::ios::binary);
                int result = 0;
                if (extension == "bmp")
                {
                    result = stbi_write_bmp_to_func(writeBytes, &file, 1, 1, 3, rgb.data());
                }
                else if (extension == "tga")
                {
                    result = stbi_write_tga_to_func(writeBytes, &file, 1, 1, 3, rgb.data());
                }
                else
                {
                    result = stbi_write_jpg_to_func(writeBytes, &file, 1, 1, 3, rgb.data(), 100);
                }
                require(result != 0 && static_cast<bool>(file), "Cannot generate RGB fixture");
            }
            const auto image = ImageLoader::load(path);
            // GLB内嵌JPEG走内存入口，必须与同一文件解码得到完全相同的像素。
            std::ifstream encodedFile(path, std::ios::binary);
            const std::vector<unsigned char> encoded{
                std::istreambuf_iterator<char>(encodedFile), std::istreambuf_iterator<char>()};
            const auto memoryImage = ImageLoader::loadMemory(encoded);
            require(memoryImage.width == image.width && memoryImage.height == image.height &&
                memoryImage.pixels == image.pixels, "Memory and file decoding differ");
            require(image.width == 1 && image.height == 1 && image.pixels[3] == 255,
                "RGB image dimensions or alpha incorrect");
            for (size_t channel = 0; channel < 3; ++channel)
            {
                require(std::abs(static_cast<int>(image.pixels[channel]) - rgb[channel]) <= 3,
                    "RGB image colors incorrect");
            }
        }

        // 兼容P3注释和非255最大值，颜色缩放与翻转都应正确。
        const auto ppmPath = directory / "scaled.ppm";
        writeText(ppmPath, "P3\n1 2\n100 # maximum\n100 0 0# top\n0 100 0\n");
        require(ImageLoader::load(ppmPath).pixels ==
            std::vector<unsigned char>({0, 255, 0, 255, 255, 0, 0, 255}), "P3 decoding failed");

        ImageLoadOptions limited;
        limited.maxDimension = 1;
        expectFailure(pngPath, limited);
        limited = {};
        limited.maxDecodedBytes = 15; // 2x2 RGBA需要16字节。
        expectFailure(pngPath, limited);
        limited = {};
        limited.maxFileBytes = 1;
        expectFailure(pngPath, limited);
        limited = {};
        limited.maxDecodedBytes = 16;
        require(ImageLoader::load(pngPath, limited).pixels == flipped.pixels,
            "Exact byte limit should allow the image");

        const auto badPath = directory / "invalid.ppm";
        for (const std::string text : {"P3\n0 1\n255\n", "P3\n1 1\n255\n0 0\n",
            "P3\n1 1\n255\n256 0 0\n", "P3\n1 1\n255\n12abc 0 0\n",
            "P3\n2147483647 2147483647\n255\n", "P3\n1 1\n0\n", "not an image", ""})
        {
            writeText(badPath, text);
            expectFailure(badPath);
        }
        // 保留PNG签名但截断数据，验证解码错误能被捕获。
        const auto truncated = directory / "truncated.png";
        std::filesystem::copy_file(pngPath, truncated, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::resize_file(truncated, 40);
        expectFailure(truncated);
        expectFailure(directory / "missing.png");

        std::cout << "Image loader tests passed: PNG/RGBA, RGB formats, P3, limits and errors" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Image loader test failed: " << exception.what() << std::endl;
        return 1;
    }
}
