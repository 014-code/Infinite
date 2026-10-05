#pragma once
#include "GltfImportContext.h"

namespace gltf
{
    // 在节点解码之后执行；把原文件节点索引换成ModelData节点索引。
    class GltfAnimationDecoder final
    {
    public:
        static void decode(GltfImportContext &context);
    };
}
