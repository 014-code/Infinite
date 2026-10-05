#include "GltfMeshDecoder.h"
#include "GltfSkinDecoder.h"

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace gltf
{
    namespace
    {
        void validateAttributes(const cgltf_primitive &primitive, std::size_t meshIndex,
            std::size_t primitiveIndex)
        {
            for (std::size_t index = 0; index < primitive.attributes_count; ++index)
            {
                const auto &attribute = primitive.attributes[index];
                require(attribute.index == 0 &&
                    (attribute.type == cgltf_attribute_type_position ||
                     attribute.type == cgltf_attribute_type_normal ||
                     attribute.type == cgltf_attribute_type_texcoord ||
                     attribute.type == cgltf_attribute_type_color ||
                     attribute.type == cgltf_attribute_type_tangent ||
                     attribute.type == cgltf_attribute_type_joints ||
                     attribute.type == cgltf_attribute_type_weights),
                    "Mesh[" + std::to_string(meshIndex) + "] Primitive[" +
                    std::to_string(primitiveIndex) + "] unsupported vertex attribute: " +
                    std::string(attribute.name != nullptr ? attribute.name : "unknown"));
            }
        }

        void generateFlatNormals(ModelPrimitiveData &primitive, GltfImportContext &context,
            std::size_t meshIndex, std::size_t primitiveIndex)
        {
            // 没有法线时拆开三角形顶点生成平面法线，不伪造跨三角形的平滑法线。
            context.consumeVertices(primitive.mesh.indices.size(),
                "generated normal vertices for Mesh[" + std::to_string(meshIndex) +
                "] Primitive[" + std::to_string(primitiveIndex) + "]");
            // 展开数组和原顶点数组短暂同时存活，两份都计入累计预算。
            context.consumeArray<Vertex>(primitive.mesh.indices.size(), "generated normal vertex bytes");
            require(primitive.mesh.indices.size() <= std::numeric_limits<std::uint32_t>::max(),
                "Generated normal indices exceed uint32 range");
            std::vector<Vertex> flat;
            std::vector<SkinVertex> skin;
            if (!primitive.skinVertices.empty())
            {
                context.consumeArray<SkinVertex>(primitive.mesh.indices.size(), "expanded skin attributes");
                skin.reserve(primitive.mesh.indices.size());
            }
            flat.reserve(primitive.mesh.indices.size());
            for (std::size_t index = 0; index < primitive.mesh.indices.size(); index += 3)
            {
                Vertex a = primitive.mesh.vertices[primitive.mesh.indices[index]];
                Vertex b = primitive.mesh.vertices[primitive.mesh.indices[index + 1]];
                Vertex c = primitive.mesh.vertices[primitive.mesh.indices[index + 2]];
                const auto face = glm::cross(b.position - a.position, c.position - a.position);
                const auto length = glm::length(face);
                require(std::isfinite(length) && length > 0.0f,
                    "Mesh[" + std::to_string(meshIndex) + "] Primitive[" +
                    std::to_string(primitiveIndex) + "] has a degenerate triangle without normals");
                a.normal = b.normal = c.normal = face / length;
                flat.insert(flat.end(), {a, b, c});
                if (!primitive.skinVertices.empty())
                {
                    for (std::size_t corner = 0; corner < 3; ++corner)
                    { skin.push_back(primitive.skinVertices[primitive.mesh.indices[index + corner]]); }
                }
            }
            primitive.mesh.vertices = std::move(flat);
            primitive.skinVertices = std::move(skin);
            for (std::size_t index = 0; index < primitive.mesh.indices.size(); ++index)
            {
                primitive.mesh.indices[index] = static_cast<std::uint32_t>(index);
            }
        }
    }

    void GltfMeshDecoder::decode(GltfImportContext &context)
    {
        const auto &data = *context.document;
        context.consumeArray<std::vector<std::size_t>>(data.meshes_count, "mesh primitive mapping");
        context.meshPrimitives.resize(data.meshes_count);

        std::size_t primitiveCount = 0;
        for (std::size_t index = 0; index < data.meshes_count; ++index)
        {
            const auto count = data.meshes[index].primitives_count;
            context.consumeArray<ModelPrimitiveData>(count, "primitive descriptors");
            context.consumeArray<std::size_t>(count, "primitive mapping entries");
            require(count <= std::numeric_limits<std::size_t>::max() - primitiveCount,
                "Primitive count overflow");
            primitiveCount += count;
            context.meshPrimitives[index].reserve(count);
        }
        context.result.primitives.reserve(primitiveCount);

        for (std::size_t meshIndex = 0; meshIndex < data.meshes_count; ++meshIndex)
        {
            const auto &mesh = data.meshes[meshIndex];
            require(mesh.weights_count == 0,
                "Mesh[" + std::to_string(meshIndex) + "] morph weights are not supported");
            for (std::size_t primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex)
            {
                const auto &source = mesh.primitives[primitiveIndex];
                const std::string prefix = "Mesh[" + std::to_string(meshIndex) + "] Primitive[" +
                    std::to_string(primitiveIndex) + "] ";
                require(source.type == cgltf_primitive_type_triangles &&
                    !source.has_draco_mesh_compression && source.targets_count == 0,
                    prefix + "only uncompressed TRIANGLES without morph targets are supported");

                const auto *position = cgltf_find_accessor(&source, cgltf_attribute_type_position, 0);
                const auto *normal = cgltf_find_accessor(&source, cgltf_attribute_type_normal, 0);
                const auto *uv = cgltf_find_accessor(&source, cgltf_attribute_type_texcoord, 0);
                const auto *color = cgltf_find_accessor(&source, cgltf_attribute_type_color, 0);
                const auto *tangent = cgltf_find_accessor(&source, cgltf_attribute_type_tangent, 0);
                require(position != nullptr && position->type == cgltf_type_vec3 &&
                    position->component_type == cgltf_component_type_r_32f && !position->normalized,
                    prefix + "POSITION must be float VEC3");
                validateAttributes(source, meshIndex, primitiveIndex);
                require(normal == nullptr || (normal->count == position->count &&
                    normal->type == cgltf_type_vec3 &&
                    normal->component_type == cgltf_component_type_r_32f && !normal->normalized),
                    prefix + "invalid NORMAL accessor");
                require(uv == nullptr || (uv->count == position->count && uv->type == cgltf_type_vec2 &&
                    ((uv->component_type == cgltf_component_type_r_32f && !uv->normalized) ||
                     ((uv->component_type == cgltf_component_type_r_8u ||
                       uv->component_type == cgltf_component_type_r_16u) && uv->normalized))),
                    prefix + "invalid TEXCOORD_0 accessor");
                require(color == nullptr || (color->count == position->count &&
                    (color->type == cgltf_type_vec3 || color->type == cgltf_type_vec4) &&
                    ((color->component_type == cgltf_component_type_r_32f && !color->normalized) ||
                     ((color->component_type == cgltf_component_type_r_8u || color->component_type == cgltf_component_type_r_16u) && color->normalized))),
                    prefix + "invalid COLOR_0 accessor");
                require(tangent == nullptr || (normal != nullptr && tangent->count == position->count &&
                    tangent->type == cgltf_type_vec4 && tangent->component_type == cgltf_component_type_r_32f && !tangent->normalized),
                    prefix + "invalid TANGENT accessor (requires NORMAL)");

                context.consumeVertices(position->count, prefix + "vertices");
                ModelPrimitiveData target;
                target.material = source.material != nullptr ?
                    cgltf_material_index(context.document.get(), source.material) :
                    context.result.materials.size() - 1;
                require(target.material < context.result.materials.size(),
                    prefix + "material index is out of range");
                const auto &surface = context.result.materials[target.material];
                bool usesTexture = surface.texture >= 0;
                for (auto texture : surface.pbrTextures) { usesTexture |= texture >= 0; }
                require(uv != nullptr || !usesTexture,
                    prefix + "textured primitive has no TEXCOORD_0");

                context.consumeArray<Vertex>(position->count, prefix + "vertex bytes");
                target.mesh.vertices.resize(position->count);
                for (std::size_t vertexIndex = 0; vertexIndex < position->count; ++vertexIndex)
                {
                    auto &vertex = target.mesh.vertices[vertexIndex];
                    require(cgltf_accessor_read_float(position, vertexIndex,
                        glm::value_ptr(vertex.position), 3), prefix + "cannot read POSITION");
                    if (normal != nullptr)
                    {
                        require(cgltf_accessor_read_float(normal, vertexIndex,
                            glm::value_ptr(vertex.normal), 3), prefix + "cannot read NORMAL");
                    }
                    if (uv != nullptr)
                    {
                        require(cgltf_accessor_read_float(uv, vertexIndex,
                            glm::value_ptr(vertex.uv), 2), prefix + "cannot read TEXCOORD_0");
                    }
                    if (color != nullptr)
                    {
                        glm::vec4 rgba(1);
                        require(cgltf_accessor_read_float(color, vertexIndex, glm::value_ptr(rgba), 4), prefix + "cannot read COLOR_0");
                        if (color->type == cgltf_type_vec3) { rgba.a = 1; }
                        for (int channel = 0; channel < 4; ++channel)
                        { require(std::isfinite(rgba[channel]) && rgba[channel] >= 0 && rgba[channel] <= 1, prefix + "invalid COLOR_0 value"); }
                        vertex.color = glm::vec3(rgba); vertex.colorAlpha = rgba.a;
                    }
                    if (tangent != nullptr)
                    {
                        require(cgltf_accessor_read_float(tangent, vertexIndex, glm::value_ptr(vertex.tangent), 4), prefix + "cannot read TANGENT");
                        for (int component = 0; component < 4; ++component)
                        { require(std::isfinite(vertex.tangent[component]), prefix + "non-finite TANGENT"); }
                        require(std::abs(glm::length(glm::vec3(vertex.tangent)) - 1) < .001f &&
                            std::abs(vertex.tangent.w) == 1, prefix + "TANGENT must have unit xyz and w=+/-1");
                    }
                    for (const float value : {vertex.position.x, vertex.position.y, vertex.position.z,
                        vertex.normal.x, vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y})
                    {
                        require(std::isfinite(value), prefix + "contains a non-finite vertex value");
                    }
                }

                const auto *indices = source.indices;
                const auto indexCount = indices != nullptr ? indices->count : position->count;
                context.consumeIndices(indexCount, prefix + "indices");
                require(indexCount % 3 == 0, prefix + "has an incomplete triangle");
                if (indices != nullptr)
                {
                    require(indices->type == cgltf_type_scalar && !indices->normalized &&
                        (indices->component_type == cgltf_component_type_r_8u ||
                         indices->component_type == cgltf_component_type_r_16u ||
                         indices->component_type == cgltf_component_type_r_32u),
                        prefix + "has an invalid index component type");
                }
                context.consumeArray<std::uint32_t>(indexCount, prefix + "index bytes");
                target.mesh.indices.reserve(indexCount);
                for (std::size_t index = 0; index < indexCount; ++index)
                {
                    const auto value = indices != nullptr ? cgltf_accessor_read_index(indices, index) : index;
                    require(value < position->count && value < std::numeric_limits<std::uint32_t>::max(),
                        prefix + "index is out of range");
                    target.mesh.indices.push_back(static_cast<std::uint32_t>(value));
                }
                try { target.skinVertices = GltfSkinDecoder::vertices(source, position->count, context); }
                catch (const std::exception &error) { throw std::runtime_error(prefix + error.what()); }
                if (normal == nullptr) { generateFlatNormals(target, context, meshIndex, primitiveIndex); }

                context.meshPrimitives[meshIndex].push_back(context.result.primitives.size());
                context.result.primitives.push_back(std::move(target));
            }
        }
    }
}
