#include "GltfValidator.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace gltf
{
    namespace
    {
        void validateAccessor(const cgltf_accessor &accessor, std::size_t index)
        {
            require(!accessor.is_sparse,
                "Accessor[" + std::to_string(index) + "] sparse accessors are not supported");
            require(accessor.buffer_view != nullptr && accessor.count > 0,
                "Accessor[" + std::to_string(index) + "] requires a buffer view and positive count");

            // 先用除法检查最后一个元素的位置，避免损坏的count/stride触发乘法溢出。
            const auto element = cgltf_calc_size(accessor.type, accessor.component_type);
            const auto length = accessor.buffer_view->size;
            require(element > 0 && accessor.stride >= element && accessor.offset <= length &&
                element <= length - accessor.offset,
                "Accessor[" + std::to_string(index) + "] has invalid offset/stride");
            require(accessor.count - 1 <=
                (length - accessor.offset - element) / accessor.stride,
                "Accessor[" + std::to_string(index) + "] exceeds its buffer view");

            const auto component = cgltf_component_size(accessor.component_type);
            require(component != 0 &&
                (accessor.offset + accessor.buffer_view->offset) % component == 0 &&
                accessor.stride % component == 0,
                "Accessor[" + std::to_string(index) + "] is unaligned");
        }
    }

    void GltfValidator::validate(GltfImportContext &context)
    {
        const auto &data = *context.document;
        require(data.asset.version != nullptr && std::strcmp(data.asset.version, "2.0") == 0,
            "Expected glTF 2.0");
        require(data.asset.min_version == nullptr ||
            std::strcmp(data.asset.min_version, "2.0") == 0,
            "Unsupported minimum glTF version");
        require(data.nodes_count <= context.options.maxNodes, "glTF node limit exceeded");
        require(data.skins_count <= context.options.maxNodes, "Skin count limit exceeded");
        require(data.skins_count == 0 || context.options.pbrMaterials,
            "Skinned models require loadPbrModel / pbrMaterials (preview shaders cannot deform vertices)");

        for (std::size_t index = 0; index < data.extensions_required_count; ++index)
        {
            require(std::strcmp(data.extensions_required[index], "KHR_materials_unlit") == 0,
                "Unsupported required extension: " +
                std::string(data.extensions_required[index]));
        }
        for (std::size_t index = 0; index < data.extensions_used_count; ++index)
        {
            if (std::strcmp(data.extensions_used[index], "KHR_materials_unlit") != 0)
            {
                context.result.warnings.push_back("Base-color preview ignores optional extension: " +
                    std::string(data.extensions_used[index]));
            }
        }

        for (std::size_t index = 0; index < data.buffer_views_count; ++index)
        {
            const auto &view = data.buffer_views[index];
            require(!view.has_meshopt_compression,
                "BufferView[" + std::to_string(index) + "] meshopt compression is not supported");
            require(view.buffer != nullptr && view.offset <= view.buffer->size &&
                view.size <= view.buffer->size - view.offset,
                "BufferView[" + std::to_string(index) + "] exceeds its buffer");
        }
        for (std::size_t index = 0; index < data.accessors_count; ++index)
        {
            validateAccessor(data.accessors[index], index);
        }
        require(cgltf_validate(context.document.get()) == cgltf_result_success,
            "Invalid glTF structure/accessor data");
    }
}
