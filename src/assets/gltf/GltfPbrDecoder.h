#pragma once
#include "GltfImportContext.h"

namespace gltf
{
    // 只转换单份PBR材质描述，不创建Texture/Shader，不修改全局渲染状态。
    void decodePbrMaterial(const cgltf_material &source, ModelMaterialData &target,
        const GltfImportContext &context, std::size_t index);
}
