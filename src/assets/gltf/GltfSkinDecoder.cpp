#include "GltfSkinDecoder.h"
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <set>

namespace gltf
{
    std::vector<SkinVertex> GltfSkinDecoder::vertices(const cgltf_primitive &source,
        std::size_t vertexCount, GltfImportContext &context)
    {
        const auto *joints = cgltf_find_accessor(&source, cgltf_attribute_type_joints, 0);
        const auto *weights = cgltf_find_accessor(&source, cgltf_attribute_type_weights, 0);
        if (!joints && !weights) { return {}; }
        require(joints && weights, "JOINTS_0 and WEIGHTS_0 must appear together");
        require(joints->count == vertexCount && joints->type == cgltf_type_vec4 && !joints->normalized &&
            (joints->component_type == cgltf_component_type_r_8u || joints->component_type == cgltf_component_type_r_16u),
            "JOINTS_0 must be unsigned byte/short VEC4");
        require(weights->count == vertexCount && weights->type == cgltf_type_vec4 &&
            ((weights->component_type == cgltf_component_type_r_32f && !weights->normalized) ||
             ((weights->component_type == cgltf_component_type_r_8u || weights->component_type == cgltf_component_type_r_16u) && weights->normalized)),
            "WEIGHTS_0 must be float or normalized unsigned VEC4");
        context.consumeArray<SkinVertex>(vertexCount, "skin vertex attributes");
        std::vector<SkinVertex> result(vertexCount);
        for (std::size_t index = 0; index < vertexCount; ++index)
        {
            cgltf_uint values[4]{};
            require(cgltf_accessor_read_uint(joints, index, values, 4) &&
                cgltf_accessor_read_float(weights, index, glm::value_ptr(result[index].weights), 4),
                "Cannot read skin vertex attributes");
            float sum = 0;
            for (int component = 0; component < 4; ++component)
            {
                result[index].joints[component] = values[component];
                const auto weight = result[index].weights[component];
                require(std::isfinite(weight) && weight >= 0 && weight <= 1, "Skin weight must be in [0,1]");
                sum += weight;
            }
            // 允许归一化整数解码带来的量化误差，但不把任意坏数据悄悄归一化成合法权重。
            require(std::abs(sum - 1.0f) <= .01f, "Skin weights must sum to one");
            result[index].weights /= sum;
        }
        return result;
    }

    void GltfSkinDecoder::decode(GltfImportContext &context)
    {
        const auto &data = *context.document;
        context.consumeArray<Skin>(data.skins_count, "skins");
        context.result.skins.resize(data.skins_count);
        for (std::size_t index = 0; index < data.skins_count; ++index)
        {
            const auto &source = data.skins[index];
            auto &target = context.result.skins[index];
            const std::string prefix = "Skin[" + std::to_string(index) + "] ";
            require(source.joints_count > 0 && source.joints_count <= 64, prefix + "joint count must be in [1,64]");
            context.consumeArray<std::size_t>(source.joints_count, prefix + "joint indices");
            context.consumeArray<glm::mat4>(source.joints_count, prefix + "inverse-bind matrices");
            const auto *matrices = source.inverse_bind_matrices;
            require(!matrices || (matrices->count == source.joints_count && matrices->type == cgltf_type_mat4 &&
                matrices->component_type == cgltf_component_type_r_32f && !matrices->normalized),
                prefix + "invalid inverseBindMatrices accessor");
            std::set<std::size_t> seen;
            for (std::size_t joint = 0; joint < source.joints_count; ++joint)
            {
                require(source.joints[joint] != nullptr, prefix + "missing joint node");
                const auto sourceIndex = cgltf_node_index(context.document.get(), source.joints[joint]);
                require(seen.insert(sourceIndex).second, prefix + "duplicate joint node");
                const int mapped = context.nodeMapping.at(sourceIndex);
                require(mapped >= 0, prefix + "joint lies outside the selected scene");
                target.joints.push_back(static_cast<std::size_t>(mapped));
                glm::mat4 matrix(1);
                require(!matrices || cgltf_accessor_read_float(matrices, joint, glm::value_ptr(matrix), 16),
                    prefix + "cannot read inverseBindMatrices");
                for (int column = 0; column < 4; ++column)
                {
                    for (int row = 0; row < 4; ++row)
                    { require(std::isfinite(matrix[column][row]), prefix + "non-finite inverseBindMatrix"); }
                }
                require(glm::abs(matrix[0].w) < 1e-6f && glm::abs(matrix[1].w) < 1e-6f &&
                    glm::abs(matrix[2].w) < 1e-6f && glm::abs(matrix[3].w - 1) < 1e-6f,
                    prefix + "inverseBindMatrix must be affine");
                target.inverseBind.push_back(matrix);
            }
        }
        for (const auto &node : context.result.nodes)
        {
            if (node.skin < 0) { continue; }
            require(!node.primitives.empty(), "Skinned node has no mesh primitives");
            const auto &skin = context.result.skins.at(static_cast<std::size_t>(node.skin));
            for (auto index : node.primitives)
            {
                const auto &vertices = context.result.primitives[index].skinVertices;
                require(!vertices.empty(), "Skinned primitive has no JOINTS_0/WEIGHTS_0");
                for (const auto &vertex : vertices)
                {
                    for (int component = 0; component < 4; ++component)
                    { require(vertex.joints[component] < skin.joints.size(), "Vertex joint index exceeds node skin"); }
                }
            }
        }
    }
}
