#pragma once

#include "assets/GltfLoader.h"
#include "assets/ModelData.h"

// cgltf只在assets/gltf内部使用。这个头文件不是引擎公开接口，
// 因此第三方解析器类型不会泄漏到上层资源管理和Scene模块。
#include <cgltf.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

namespace gltf
{
    // cgltf_data由cgltf_free释放；用智能指针保证解析失败或后续校验抛异常时仍能清理。
    struct CgltfDataDeleter
    {
        void operator()(cgltf_data *data) const noexcept
        {
            if (data != nullptr) { cgltf_free(data); }
        }
    };

    // 文件、解析器、输出数组和生成网格共享累计预算，释放后不返还额度。
    // 预算统计请求的数据字节，不包括分配器开销、容器额外容量、诊断字符串和调用栈；
    // 它不是整个进程的物理内存峰值限制。GPU/图片解码阶段另有自己的限制。
    struct GltfBudget
    {
        std::size_t remainingBytes;
        std::size_t verticesLeft;
        std::size_t indicesLeft;
    };

    class GltfImportContext final
    {
    public:
        GltfImportContext(const std::filesystem::path &sourcePath, const ModelLoadOptions &options)
            : sourcePath(sourcePath), options(options),
              budget{options.maxTotalBytes, options.maxVertices, options.maxIndices}
        {
        }

        // cgltf回调借用budget的地址，所以不能把上下文移动到另一个地址。
        GltfImportContext(const GltfImportContext &) = delete;
        GltfImportContext &operator=(const GltfImportContext &) = delete;
        GltfImportContext(GltfImportContext &&) = delete;
        GltfImportContext &operator=(GltfImportContext &&) = delete;

        // 统一的预算扣减入口。所有“会复制或生成数据”的模块都必须经过这里。
        void consume(std::size_t amount, const std::string &description)
        {
            if (amount > budget.remainingBytes)
            {
                throw std::runtime_error("glTF limit exceeded: " + description);
            }
            budget.remainingBytes -= amount;
        }

        // 必须先除后乘，恶意count即使接近SIZE_MAX，也不能通过整数溢出绕过预算。
        template<class T>
        void consumeArray(std::size_t count, const std::string &description)
        {
            if (count > budget.remainingBytes / sizeof(T))
            {
                throw std::runtime_error("glTF limit exceeded: " + description);
            }
            consume(count * sizeof(T), description);
        }

        void consumeVertices(std::size_t amount, const std::string &description)
        {
            if (amount > budget.verticesLeft)
            {
                throw std::runtime_error("glTF vertex limit exceeded: " + description);
            }
            budget.verticesLeft -= amount;
        }

        void consumeIndices(std::size_t amount, const std::string &description)
        {
            if (amount > budget.indicesLeft)
            {
                throw std::runtime_error("glTF index limit exceeded: " + description);
            }
            budget.indicesLeft -= amount;
        }

        const std::filesystem::path sourcePath;
        const ModelLoadOptions options;
        GltfBudget budget;

        // sourceBytes必须一直活到cgltf_data释放，因为cgltf的部分字符串和GLB数据仍借用它。
        std::vector<unsigned char> sourceBytes;

        // 外部buffer复制到这里后，再把cgltf_buffer::data指向稳定的vector内存。
        std::vector<std::vector<unsigned char>> buffers;

        // 每个cgltf mesh对应ModelData中若干primitive的索引，节点解码阶段会引用它。
        std::vector<std::vector<std::size_t>> meshPrimitives;
        // 原文件节点 -> ModelData节点；-1表示节点不属于当前选中的scene。
        std::vector<int> nodeMapping;

        // 各解码器按阶段向同一个结果快照写入；最终由takeResult移动给GltfLoader调用方。
        ModelData result;

        // C++按成员声明的逆序析构：先释放解析器，之后再释放其借用的buffers/sourceBytes，
        // 最后才销毁budget。buffer的data_free_method保持none，cgltf不会释放vector内存。
        std::unique_ptr<cgltf_data, CgltfDataDeleter> document{nullptr};

        ModelData takeResult() &&
        {
            return std::move(result);
        }
    };

    // 统一失败入口，减少各解码器中重复的if/throw样板。
    inline void require(bool condition, const std::string &message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }
}
