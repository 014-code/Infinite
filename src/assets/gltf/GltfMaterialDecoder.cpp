#include "GltfMaterialDecoder.h"
#include "GltfPbrDecoder.h"

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <string>

namespace gltf
{
    namespace
    {
        constexpr int kWrapRepeat = 10497;
        constexpr int kWrapClampToEdge = 33071;
        constexpr int kWrapMirroredRepeat = 33648;
        constexpr int kFilterNearest = 9728;
        constexpr int kFilterLinear = 9729;
        constexpr int kFilterNearestMipmapNearest = 9984;
        constexpr int kFilterLinearMipmapNearest = 9985;
        constexpr int kFilterNearestMipmapLinear = 9986;
        constexpr int kFilterLinearMipmapLinear = 9987;

        bool isValidWrapMode(int value)
        {
            return value == kWrapRepeat || value == kWrapClampToEdge ||
                value == kWrapMirroredRepeat;
        }

        bool isValidMinFilter(int value)
        {
            return value == kFilterNearest || value == kFilterLinear ||
                value == kFilterNearestMipmapNearest || value == kFilterLinearMipmapNearest ||
                value == kFilterNearestMipmapLinear || value == kFilterLinearMipmapLinear;
        }

        bool isFinite(const float *values, std::size_t count)
        {
            for (std::size_t index = 0; index < count; ++index)
            {
                if (!std::isfinite(values[index])) { return false; }
            }
            return true;
        }
    }

    void GltfMaterialDecoder::decode(GltfImportContext &context)
    {
        const auto &data = *context.document;
        context.result.pbrMaterials = context.options.pbrMaterials;

        context.consumeArray<ModelTextureData>(data.textures_count, "texture descriptors");
        context.result.textures.reserve(data.textures_count);
        for (std::size_t index = 0; index < data.textures_count; ++index)
        {
            const auto &texture = data.textures[index];
            require(texture.image != nullptr && !texture.has_basisu && !texture.has_webp,
                "Texture[" + std::to_string(index) + "] uses an unsupported encoding");

            ModelTextureData target;
            target.image = cgltf_image_index(context.document.get(), texture.image);
            if (texture.sampler != nullptr)
            {
                const auto &sampler = *texture.sampler;
                if (sampler.min_filter != 0) { target.minFilter = sampler.min_filter; }
                if (sampler.mag_filter != 0) { target.magFilter = sampler.mag_filter; }
                target.wrapS = sampler.wrap_s;
                target.wrapT = sampler.wrap_t;
            }
            require(isValidWrapMode(target.wrapS) && isValidWrapMode(target.wrapT) &&
                (target.magFilter == kFilterNearest || target.magFilter == kFilterLinear) &&
                isValidMinFilter(target.minFilter),
                "Texture[" + std::to_string(index) + "] has an invalid sampler");
            require(target.image < context.result.images.size(),
                "Texture[" + std::to_string(index) + "] image index is out of range");
            context.result.textures.push_back(target);
        }

        context.consumeArray<ModelMaterialData>(data.materials_count, "material descriptors");
        context.consumeArray<ModelMaterialData>(1, "default material descriptor");
        context.result.materials.reserve(data.materials_count + 1);
        for (std::size_t index = 0; index < data.materials_count; ++index)
        {
            const auto &material = data.materials[index];
            require(context.options.pbrMaterials || material.alpha_mode == cgltf_alpha_mode_opaque,
                "Material[" + std::to_string(index) + "] MASK/BLEND materials are not supported");
            require(!material.has_pbr_specular_glossiness,
                "Material[" + std::to_string(index) + "] specular-glossiness is not supported");

            const auto &base = material.pbr_metallic_roughness;
            require(isFinite(base.base_color_factor, 4),
                "Material[" + std::to_string(index) + "] has a non-finite base color");
            ModelMaterialData target;
            target.baseColor = glm::make_vec4(base.base_color_factor);
            for (int channel = 0; channel < 4; ++channel)
            {
                require(target.baseColor[channel] >= 0.0f && target.baseColor[channel] <= 1.0f,
                    "Material[" + std::to_string(index) + "] has an invalid baseColorFactor");
            }
            target.doubleSided = material.double_sided;
            require(!base.base_color_texture.has_transform && base.base_color_texture.texcoord == 0,
                "Material[" + std::to_string(index) + "] uses unsupported texture coordinates/transform");
            if (base.base_color_texture.texture != nullptr)
            {
                target.texture = static_cast<int>(cgltf_texture_index(context.document.get(),
                    base.base_color_texture.texture));
                require(target.texture >= 0 &&
                    static_cast<std::size_t>(target.texture) < context.result.textures.size(),
                    "Material[" + std::to_string(index) + "] texture index is out of range");
            }
            if (context.options.pbrMaterials) { decodePbrMaterial(material, target, context, index); }
            context.result.materials.push_back(target);
            if (!context.options.pbrMaterials && !material.unlit)
            {
                context.result.warnings.push_back("Material " + std::to_string(index) +
                    ": base-color preview only; PBR, normal, occlusion and emission are not rendered");
            }
        }

        // primitive可以不指定材质。默认材质放在数组末尾，避免错误地引用材质0。
        context.result.materials.push_back({});
        context.result.materials.back().pbr.metallic = 1;
        context.result.materials.back().pbr.roughness = 1;
    }
}
