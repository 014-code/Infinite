#include "GltfFileReader.h"

// cgltf实现只编译一次，放在文件读取模块中；其他glTF模块只看到声明。
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

namespace gltf
{
    namespace
    {
        // cgltf在解析过程中遇到自定义分配器返回nullptr时，部分旧版本的错误清理路径
        // 可能继续访问尚未完整初始化的数组。这里先用“只统计、不限额”的分配器完成
        // 一次解析，得到本次文档实际需要的解析器字节数，再由引擎预算统一记账。
        // 这样低预算会得到正常的可诊断错误，而不会把第三方解析器推入不安全的半解析状态。
        struct ParserAllocationStats
        {
            std::size_t totalBytes = 0;
            bool counterOverflow = false;
        };

        void *countingAlloc(void *user, cgltf_size bytes)
        {
            auto &stats = *static_cast<ParserAllocationStats *>(user);
            void *pointer = std::malloc(bytes);
            if (pointer != nullptr)
            {
                // 分配回调不能向cgltf抛C++异常；极端计数溢出留给解析返回后的普通错误路径处理。
                if (bytes > std::numeric_limits<std::size_t>::max() - stats.totalBytes)
                {
                    stats.counterOverflow = true;
                }
                else
                {
                    stats.totalBytes += bytes;
                }
            }
            return pointer;
        }

        void countingFree(void *, void *pointer)
        {
            std::free(pointer);
        }

        std::vector<unsigned char> readBinaryFile(GltfImportContext &context,
            const std::filesystem::path &path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            require(bool(file), "Cannot open glTF dependency: " + path.u8string());
            const auto length = file.tellg();
            require(length > 0 && static_cast<unsigned long long>(length) <= context.options.maxFileBytes,
                "Invalid or oversized glTF file: " + path.u8string());

            const auto size = static_cast<std::size_t>(length);
            context.consume(size, "encoded bytes for " + path.u8string());
            std::vector<unsigned char> bytes(size);
            file.seekg(0);
            require(bool(file.read(reinterpret_cast<char *>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()))),
                "Incomplete glTF read: " + path.u8string());
            return bytes;
        }

        int hexDigit(char value)
        {
            if (value >= '0' && value <= '9') { return value - '0'; }
            if (value >= 'A' && value <= 'F') { return value - 'A' + 10; }
            if (value >= 'a' && value <= 'f') { return value - 'a' + 10; }
            return -1;
        }

        std::vector<unsigned char> decodeBase64(std::string_view payload,
            GltfImportContext &context)
        {
            require(!payload.empty() && payload.size() % 4 == 0, "Malformed base64 length");
            std::size_t padding = payload.back() == '=' ? 1 : 0;
            if (payload.size() >= 2 && payload[payload.size() - 2] == '=') { ++padding; }
            require(padding <= 2, "Malformed base64 padding");

            const std::size_t size = payload.size() / 4 * 3 - padding;
            require(size <= context.options.maxFileBytes, "Oversized data URI");
            context.consume(size, "data URI bytes");

            constexpr char alphabet[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::vector<unsigned char> result;
            result.reserve(size);
            for (std::size_t offset = 0; offset < payload.size(); offset += 4)
            {
                unsigned bits = 0;
                for (std::size_t index = 0; index < 4; ++index)
                {
                    const char encoded = payload[offset + index];
                    const bool isPadding = offset + index >= payload.size() - padding;
                    if (isPadding)
                    {
                        require(encoded == '=', "Malformed base64 padding");
                        bits <<= 6;
                    }
                    else
                    {
                        const char *found = std::find(alphabet, alphabet + 64, encoded);
                        require(found != alphabet + 64, "Malformed base64 character");
                        bits = (bits << 6) + static_cast<unsigned>(found - alphabet);
                    }
                }
                for (int shift : {16, 8, 0})
                {
                    if (result.size() < size)
                    {
                        result.push_back(static_cast<unsigned char>((bits >> shift) & 0xff));
                    }
                }
            }
            return result;
        }

        std::vector<unsigned char> readUri(GltfImportContext &context, const char *value)
        {
            require(value != nullptr, "Missing glTF URI");
            // URI借用解析器字符串；尤其是base64数据，不为substr再复制一份大字符串。
            const std::string_view uri(value);
            if (uri.rfind("data:", 0) == 0)
            {
                const auto comma = uri.find(',');
                require(comma != std::string::npos && comma >= 7 &&
                    uri.substr(comma - 7, 7) == ";base64",
                    "Only base64 data URI is supported");
                return decodeBase64(uri.substr(comma + 1), context);
            }

            // glTF URI先做百分号解码，再限制为本地相对路径，支持中文和空格文件名。
            // 允许../引用相邻资源目录；这是相对路径规则，不是目录沙箱。
            std::string decoded;
            context.consume(uri.size(), "decoded URI path bytes");
            decoded.reserve(uri.size());
            for (std::size_t i = 0; i < uri.size(); ++i)
            {
                if (uri[i] != '%')
                {
                    decoded += uri[i];
                    continue;
                }
                const int high = i + 1 < uri.size() ? hexDigit(uri[i + 1]) : -1;
                const int low = i + 2 < uri.size() ? hexDigit(uri[i + 2]) : -1;
                require(high >= 0 && low >= 0, "Malformed glTF URI escape");
                decoded += static_cast<char>(high * 16 + low);
                i += 2;
            }
            require(!decoded.empty() && decoded.find_first_of(":?#\\") == std::string::npos &&
                decoded.find('\0') == std::string::npos && decoded.front() != '/',
                "Only local relative glTF URIs are supported");
            return readBinaryFile(context, context.sourcePath.parent_path() /
                std::filesystem::u8path(decoded));
        }

        void readBuffers(GltfImportContext &context)
        {
            auto &data = *context.document;
            context.consumeArray<std::vector<unsigned char>>(data.buffers_count, "buffer containers");
            context.buffers.resize(data.buffers_count);
            for (std::size_t index = 0; index < data.buffers_count; ++index)
            {
                auto &buffer = data.buffers[index];
                if (buffer.uri != nullptr)
                {
                    try { context.buffers[index] = readUri(context, buffer.uri); }
                    catch (const std::exception &error)
                    {
                        throw std::runtime_error("Buffer[" + std::to_string(index) + "] " + error.what());
                    }
                }
                else
                {
                    require(index == 0 && data.bin != nullptr && buffer.size <= data.bin_size,
                        "Missing GLB buffer at Buffer[" + std::to_string(index) + "]");
                    require(buffer.size <= context.options.maxFileBytes,
                        "Oversized GLB buffer at Buffer[" + std::to_string(index) + "]");
                    context.consume(buffer.size, "GLB buffer copy");
                    const auto *begin = static_cast<const unsigned char *>(data.bin);
                    context.buffers[index].assign(begin, begin + buffer.size);
                }
                require(buffer.size <= context.buffers[index].size(),
                    "Buffer[" + std::to_string(index) + "] is shorter than byteLength");
                buffer.data = context.buffers[index].data();
            }
        }

        bool isPng(const std::vector<unsigned char> &bytes)
        {
            constexpr unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
            return bytes.size() >= sizeof(signature) &&
                std::equal(std::begin(signature), std::end(signature), bytes.begin());
        }

        bool isJpeg(const std::vector<unsigned char> &bytes)
        {
            return bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff;
        }

        void readImagesImpl(GltfImportContext &context)
        {
            auto &data = *context.document;
            context.consumeArray<std::vector<unsigned char>>(data.images_count, "image containers");
            context.result.images.reserve(data.images_count);
            for (std::size_t index = 0; index < data.images_count; ++index)
            {
                const auto &image = data.images[index];
                std::vector<unsigned char> bytes;
                if (image.uri != nullptr)
                {
                    try { bytes = readUri(context, image.uri); }
                    catch (const std::exception &error)
                    {
                        throw std::runtime_error("Image[" + std::to_string(index) + "] " + error.what());
                    }
                }
                else
                {
                    require(image.buffer_view != nullptr,
                        "Image[" + std::to_string(index) + "] has no URI or buffer view");
                    context.consume(image.buffer_view->size,
                        "embedded image copy for Image[" + std::to_string(index) + "]");
                    const auto *begin = cgltf_buffer_view_data(image.buffer_view);
                    require(begin != nullptr, "Image[" + std::to_string(index) + "] buffer is unavailable");
                    bytes.assign(begin, begin + image.buffer_view->size);
                }
                require(isPng(bytes) || isJpeg(bytes),
                    "Image[" + std::to_string(index) + "] must be PNG or JPEG");
                context.result.images.push_back(std::move(bytes));
            }
        }
    }

    void GltfFileReader::read(GltfImportContext &context)
    {
        context.sourceBytes = readBinaryFile(context, context.sourcePath);

        cgltf_options options{};
        ParserAllocationStats parserStats;
        options.memory = {countingAlloc, countingFree, &parserStats};
        cgltf_data *raw = nullptr;
        const auto status = cgltf_parse(&options, context.sourceBytes.data(),
            context.sourceBytes.size(), &raw);
        // 即使解析失败也用智能指针接管非空结果，避免第三方库留下半成品时泄漏。
        std::unique_ptr<cgltf_data, CgltfDataDeleter> parsed(raw);
        require(status == cgltf_result_success,
            "cgltf parse failed (code " + std::to_string(status) + ")");
        require(parsed != nullptr, "cgltf returned no document");
        require(!parserStats.counterOverflow, "cgltf parser allocation counter overflow");
        context.consume(parserStats.totalBytes, "cgltf parser allocations");
        context.document = std::move(parsed);

        readBuffers(context);
    }

    void GltfFileReader::readImages(GltfImportContext &context)
    {
        readImagesImpl(context);
    }
}
