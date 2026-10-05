#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

struct ImageData
{
    // 统一输出8位RGBA，灰度/RGB图片会补齐通道，缺少Alpha时填255（不透明）。
    // pixels拥有CPU内存，与OpenGL上下文无关，可以在创建窗口前加载。
    static constexpr size_t channels = 4;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
};

struct ImageLoadOptions
{
    // 尺寸、文件大小和解码结果分别限制；这些是引擎策略，不是GPU硬件上限。
    // 调用者可以按项目预算调整。解码临时内存不包含在maxDecodedBytes内。
    int maxDimension = 16384;
    size_t maxFileBytes = 64ull * 1024 * 1024;
    size_t maxDecodedBytes = 256ull * 1024 * 1024;

    // 默认翻转到左下角原点，匹配当前示例的UV约定；其他用途可设为false。
    bool flipVertically = true;
};

class ImageLoader
{
public:
    // PNG/JPG/BMP/TGA和二进制PPM由stb解码，P3文本PPM保留兼容读取。
    // 不调用OpenGL；读取失败会抛出带文件路径的异常。
    static ImageData load(
        const std::filesystem::path &path,
        const ImageLoadOptions &options = {});

    // 从已经位于内存中的PNG/JPEG/BMP/TGA/PPM字节解码，不创建临时文件。
    // glTF的GLB内嵌图片和data URI使用此入口；debugName只用于异常信息。
    static ImageData loadMemory(
        const std::vector<unsigned char> &encoded,
        const ImageLoadOptions &options = {},
        const std::string &debugName = "<memory>");
};
