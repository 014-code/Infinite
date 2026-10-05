#pragma once

#include "GltfImportContext.h"

namespace gltf
{
    // 读取选中scene的节点树，转换TRS/矩阵，并把节点引用的mesh映射为primitive索引。
    class GltfNodeDecoder final
    {
    public:
        static void decode(GltfImportContext &context);
    };
}
