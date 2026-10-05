#pragma once

#include "GltfImportContext.h"

namespace gltf
{
    // 在任何材质/网格解码之前验证解析器数据，防止坏的offset、stride或层级数据
    // 进入后续模块。验证器只读cgltf数据，只向ModelData追加可选扩展警告。
    class GltfValidator final
    {
    public:
        static void validate(GltfImportContext &context);
    };
}
