#include "graphics/resources/ImageLoader.h"

// stb的实现只在这一个cpp中编译，避免多个文件重复定义函数。
// 本阶段只启用普通静态图片格式，HDR/动画等需要单独设计数据类型。
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_ONLY_PNM
#define STBI_NO_STDIO
#include <stb/stb_image.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    size_t checkedByteCount(int width, int height, const ImageLoadOptions &options)
    {
        if (width <= 0 || height <= 0 || width > options.maxDimension ||
            height > options.maxDimension)
        {
            throw std::runtime_error("Image dimensions exceed the configured limit");
        }

        // 先用除法检查乘法是否溢出，再分配内存；大小按输出RGBA计算。
        const size_t w = static_cast<size_t>(width);
        const size_t h = static_cast<size_t>(height);
        if (w > std::numeric_limits<size_t>::max() / ImageData::channels ||
            h > std::numeric_limits<size_t>::max() / (w * ImageData::channels))
        {
            throw std::runtime_error("Image byte count overflow");
        }
        const size_t bytes = w * h * ImageData::channels;
        if (bytes > options.maxDecodedBytes)
        {
            throw std::runtime_error("Decoded image exceeds the configured byte limit");
        }
        return bytes;
    }

    std::string readPpmToken(std::istream &input)
    {
        // '#'到行尾是PPM注释，也支持紧跟在数字后面的注释。
        std::string token;
        char character = 0;
        while (input.get(character))
        {
            if (character == '#')
            {
                input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                if (!token.empty())
                {
                    return token;
                }
            }
            else if (std::isspace(static_cast<unsigned char>(character)))
            {
                if (!token.empty())
                {
                    return token;
                }
            }
            else
            {
                token += character;
            }
        }
        if (token.empty())
        {
            throw std::runtime_error("Unexpected end of P3 image");
        }
        return token;
    }

    int readPpmInteger(std::istream &input)
    {
        const auto token = readPpmToken(input);
        int value = 0;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
        {
            throw std::runtime_error("Invalid P3 integer: " + token);
        }
        return value;
    }

    ImageData readP3(const std::vector<unsigned char> &file, const ImageLoadOptions &options)
    {
        // P3是可读的文本格式，适合学习和测试，但文件体积大、解析慢；
        // 其他格式统一交给stb_image处理。两条路径最终都输出RGBA8像素。
        std::istringstream input(std::string(file.begin(), file.end()));
        readPpmToken(input); // 已在调用处识别过P3文件头。
        ImageData image;
        image.width = readPpmInteger(input);
        image.height = readPpmInteger(input);
        const int maximumValue = readPpmInteger(input);
        if (maximumValue <= 0 || maximumValue > 65535)
        {
            throw std::runtime_error("Invalid P3 maximum color value");
        }
        image.pixels.resize(checkedByteCount(image.width, image.height, options));

        // 每次处理一个完整像素的RGB，再补齐Alpha。最大颜色值可能是255或
        // 其他合法范围，因此这里按maximumValue归一化到OpenGL常用的0~255。
        for (size_t offset = 0; offset < image.pixels.size(); offset += ImageData::channels)
        {
            for (size_t channel = 0; channel < 3; ++channel)
            {
                const int value = readPpmInteger(input);
                if (value < 0 || value > maximumValue)
                {
                    throw std::runtime_error("P3 color value out of range");
                }
                image.pixels[offset + channel] = static_cast<unsigned char>(value * 255 / maximumValue);
            }
            image.pixels[offset + 3] = 255;
        }
        return image;
    }

    std::runtime_error stbError(const char *operation)
    {
        const char *reason = stbi_failure_reason();
        return std::runtime_error(std::string(operation) + ": " + (reason ? reason : "unknown error"));
    }
}

namespace
{
    ImageData decodeMemory(const std::vector<unsigned char> &file, const ImageLoadOptions &options)
    {
        ImageData image;
        if (file.size() >= 3 && file[0] == 'P' && file[1] == '3' && std::isspace(file[2]))
        {
            // 先识别P3，避免把测试资源交给stb后得到不够具体的错误信息。
            image = readP3(file, options);
        }
        else
        {
            // stb先检查尺寸，再解码为强制RGBA；这样没有Alpha的图片也能和
            // Material的透明混合接口使用相同的数据布局。
            int sourceChannels = 0;
            if (file.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
            {
                throw std::runtime_error("Encoded image is too large for decoder");
            }
            const int length = static_cast<int>(file.size());
            if (!stbi_info_from_memory(file.data(), length, &image.width, &image.height, &sourceChannels))
            {
                throw stbError("Cannot inspect image");
            }
            const size_t bytes = checkedByteCount(image.width, image.height, options);

            // unique_ptr负责调用stbi_image_free：即使vector分配内存失败也不会泄漏。
            using DecodedPixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
            DecodedPixels decoded(stbi_load_from_memory(file.data(), length,
                &image.width, &image.height, &sourceChannels, STBI_rgb_alpha), &stbi_image_free);
            if (!decoded)
            {
                throw stbError("Cannot decode image");
            }
            if (checkedByteCount(image.width, image.height, options) != bytes)
            {
                throw std::runtime_error("Decoded dimensions disagree with image header");
            }
            image.pixels.assign(decoded.get(), decoded.get() + bytes);
        }

        // 在自己的像素容器中翻转行，不修改stb全局翻转选项，避免影响其他加载调用。
        if (options.flipVertically)
        {
            const size_t rowBytes = static_cast<size_t>(image.width) * ImageData::channels;
            for (int row = 0; row < image.height / 2; ++row)
            {
                auto top = image.pixels.begin() + static_cast<size_t>(row) * rowBytes;
                auto bottom = image.pixels.begin() + static_cast<size_t>(image.height - 1 - row) * rowBytes;
                std::swap_ranges(top, top + rowBytes, bottom);
            }
        }
        return image;
    }
}

ImageData ImageLoader::load(const std::filesystem::path &path, const ImageLoadOptions &options)
{
    try
    {
        // 一次读取文件快照，检查头部和实际解码使用同一份字节，防止文件中途变化。
        // filesystem::path也能让Windows文件流打开中文路径。
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input)
        {
            throw std::runtime_error("Cannot open image file");
        }
        const auto size = input.tellg();
        if (size <= 0 || static_cast<unsigned long long>(size) > options.maxFileBytes ||
            size > std::numeric_limits<int>::max())
        {
            throw std::runtime_error("Empty image or file exceeds the configured byte limit");
        }
        std::vector<unsigned char> file(static_cast<size_t>(size));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char *>(file.data()), static_cast<std::streamsize>(file.size())))
        {
            throw std::runtime_error("Cannot read complete image file");
        }
        return decodeMemory(file, options);
    }
    catch (const std::exception &exception)
    {
        throw std::runtime_error("Failed to load image '" + path.u8string() + "': " + exception.what());
    }
}

ImageData ImageLoader::loadMemory(const std::vector<unsigned char> &encoded,
    const ImageLoadOptions &options, const std::string &debugName)
{
    try
    {
        if (encoded.empty() || encoded.size() > options.maxFileBytes)
        {
            throw std::runtime_error("Empty image or encoded image exceeds the configured byte limit");
        }
        return decodeMemory(encoded, options);
    }
    catch (const std::exception &exception)
    {
        throw std::runtime_error("Failed to decode image '" + debugName + "': " + exception.what());
    }
}
