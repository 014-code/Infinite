#pragma once

#include "GltfImportContext.h"

namespace gltf
{
    // 将glTF的图片索引、采样器和基础颜色材质翻译成ModelData。
    // 这里不上传Texture，也不决定具体Shader如何实现PBR。
    class GltfMaterialDecoder final
    {
    public:
        static void decode(GltfImportContext &context);
    };
}
