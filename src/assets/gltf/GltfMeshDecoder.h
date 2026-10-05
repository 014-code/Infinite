#pragma once

#include "GltfImportContext.h"

namespace gltf
{
    // 将三角形primitive解码为引擎CPU网格，并处理索引、UV和缺失法线。
    // GPU上传属于ResourceManager，这个模块始终保持纯CPU、可独立测试。
    class GltfMeshDecoder final
    {
    public:
        static void decode(GltfImportContext &context);
    };
}
