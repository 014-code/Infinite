#include "GltfPbrDecoder.h"
#include <glm/gtc/type_ptr.hpp>

namespace gltf
{
    void decodePbrMaterial(const cgltf_material &source, ModelMaterialData &target,
        const GltfImportContext &context, std::size_t index)
    {
        const std::string prefix = "Material[" + std::to_string(index) + "] ";
        const auto textureIndex = [&](const cgltf_texture_view &view, const char *slot)
        {
            if (!view.texture) { return -1; }
            require(view.texcoord == 0 && !view.has_transform, prefix + slot + " requires TEXCOORD_0 without transform");
            const auto value = cgltf_texture_index(context.document.get(), view.texture);
            require(value < context.result.textures.size(), prefix + slot + " texture index out of range");
            return static_cast<int>(value);
        };
        target.pbr.metallic = source.has_pbr_metallic_roughness ? source.pbr_metallic_roughness.metallic_factor : 1;
        target.pbr.roughness = source.has_pbr_metallic_roughness ? source.pbr_metallic_roughness.roughness_factor : 1;
        target.pbr.emission = glm::make_vec3(source.emissive_factor);
        target.pbr.normalScale = source.normal_texture.texture ? source.normal_texture.scale : 1;
        target.pbr.occlusionStrength = source.occlusion_texture.texture ? source.occlusion_texture.scale : 1;
        target.pbr.alphaCutoff = source.alpha_cutoff;
        target.pbr.unlit = source.unlit;
        try { target.pbr.validate(); }
        catch (const std::exception &error) { throw std::runtime_error(prefix + error.what()); }
        for (int channel = 0; channel < 3; ++channel)
        { require(target.pbr.emission[channel] <= 1, prefix + "emissiveFactor must be in [0,1]"); }
        switch (source.alpha_mode)
        {
        case cgltf_alpha_mode_opaque: target.alphaMode = ModelAlphaMode::Opaque; break;
        case cgltf_alpha_mode_mask: target.alphaMode = ModelAlphaMode::Mask; break;
        case cgltf_alpha_mode_blend: target.alphaMode = ModelAlphaMode::Blend; break;
        default: throw std::runtime_error(prefix + "invalid alpha mode");
        }
        target.pbrTextures = {{textureIndex(source.pbr_metallic_roughness.metallic_roughness_texture, "metallicRoughness"),
            textureIndex(source.normal_texture, "normal"), textureIndex(source.occlusion_texture, "occlusion"),
            textureIndex(source.emissive_texture, "emissive")}};
    }
}
