#pragma once
#include "GltfImportContext.h"

namespace gltf
{
    class GltfSkinDecoder final
    {
    public:
        // 网格阶段只解析顶点权重；节点阶段完成映射后才解析Skin的关节引用。
        static std::vector<SkinVertex> vertices(const cgltf_primitive &source,
            std::size_t vertexCount, GltfImportContext &context);
        static void decode(GltfImportContext &context);
    };
}
