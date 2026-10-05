#include "GltfNodeDecoder.h"

#include "math/Transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <string>

namespace gltf
{
    namespace
    {
        void requireFinite(const float *values, std::size_t count, const std::string &field)
        {
            for (std::size_t index = 0; index < count; ++index)
            {
                require(std::isfinite(values[index]), "Non-finite " + field);
            }
        }

        void decodeTransform(const cgltf_node &node, ModelNodeData &result)
        {
            Transform transform;
            if (!node.has_matrix)
            {
                requireFinite(node.translation, 3, "node translation");
                requireFinite(node.scale, 3, "node scale");
                transform.position = glm::make_vec3(node.translation);
                transform.scale = glm::make_vec3(node.scale);
                // glTF保存顺序是x/y/z/w，而GLM构造函数参数顺序是w/x/y/z。
                transform.setRotation(glm::quat(node.rotation[3], node.rotation[0],
                    node.rotation[1], node.rotation[2]));
            }
            else
            {
                require(!node.has_translation && !node.has_rotation && !node.has_scale,
                    "Node combines matrix and TRS");
                requireFinite(node.matrix, 16, "node matrix");
                const auto matrix = glm::make_mat4(node.matrix);
                transform.position = glm::vec3(matrix[3]);
                glm::mat3 basis(matrix);
                // 列长度是缩放；负行列式由X轴承担符号，再从正交基恢复四元数。
                for (int column = 0; column < 3; ++column)
                {
                    transform.scale[column] = glm::length(basis[column]);
                    require(std::isfinite(transform.scale[column]) &&
                        transform.scale[column] > 1e-8f,
                        "Singular node matrix cannot be represented as editable TRS");
                }
                if (glm::determinant(basis) < 0.0f) { transform.scale.x = -transform.scale.x; }
                for (int column = 0; column < 3; ++column)
                {
                    basis[column] /= transform.scale[column];
                }
                transform.setRotation(glm::quat_cast(basis));

                // 分解后重新组合并比较，拒绝无法由当前Transform表达的shear/perspective。
                const auto rebuilt = transform.localMatrix();
                for (int column = 0; column < 4; ++column)
                {
                    for (int row = 0; row < 4; ++row)
                    {
                        require(std::abs(matrix[column][row] - rebuilt[column][row]) <=
                            1e-4f * std::max(1.0f, std::abs(matrix[column][row])),
                            "Node matrix contains unsupported shear or perspective");
                    }
                }
            }
            result.position = transform.position;
            result.rotation = transform.rotation();
            result.scale = transform.scale;
        }

        void validateParentDepth(const GltfImportContext &context)
        {
            const auto &data = *context.document;
            for (std::size_t index = 0; index < data.nodes_count; ++index)
            {
                std::size_t depth = 0;
                for (auto *node = &data.nodes[index]; node != nullptr; node = node->parent)
                {
                    require(++depth <= context.options.maxDepth,
                        "Node cycle or hierarchy depth limit exceeded at Node[" +
                        std::to_string(index) + "]");
                }
            }
        }
    }

    void GltfNodeDecoder::decode(GltfImportContext &context)
    {
        const auto &data = *context.document;
        validateParentDepth(context);
        require(data.scenes_count > 0, "glTF has no scene");
        const auto *scene = data.scene != nullptr ? data.scene : &data.scenes[0];
        context.consumeArray<unsigned char>(data.nodes_count, "node visitation flags");
        std::vector<unsigned char> visited(data.nodes_count, 0);
        context.consumeArray<ModelNodeData>(data.nodes_count, "node descriptors");
        context.result.nodes.reserve(data.nodes_count);
        context.consumeArray<int>(data.nodes_count, "source node mapping");
        context.nodeMapping.assign(data.nodes_count, -1);

        std::function<void(const cgltf_node *, int)> visit = [&](const cgltf_node *node, int parent)
        {
            const auto index = cgltf_node_index(context.document.get(), node);
            require(index < visited.size(), "Scene references an invalid node");
            require(!visited[index], "Node[" + std::to_string(index) + "] is referenced more than once");
            visited[index] = true;
            require(node->weights_count == 0 &&
                !node->has_mesh_gpu_instancing,
                "Node[" + std::to_string(index) + "] deformation/instancing is not supported");

            ModelNodeData target;
            const std::string fallback = "node_" + std::to_string(index);
            const char *name = node->name != nullptr ? node->name : fallback.c_str();
            context.consume(std::char_traits<char>::length(name), "node name bytes");
            target.name = name;
            target.parent = parent;
            if (node->skin)
            { target.skin = static_cast<int>(cgltf_skin_index(context.document.get(), node->skin)); }
            try { decodeTransform(*node, target); }
            catch (const std::exception &error)
            {
                throw std::runtime_error("Node[" + std::to_string(index) + "] " + error.what());
            }
            if (node->mesh != nullptr)
            {
                const auto meshIndex = cgltf_mesh_index(context.document.get(), node->mesh);
                require(meshIndex < context.meshPrimitives.size(),
                    "Node[" + std::to_string(index) + "] mesh index is out of range");
                context.consumeArray<std::size_t>(context.meshPrimitives[meshIndex].size(),
                    "node primitive references");
                target.primitives = context.meshPrimitives[meshIndex];
            }
            if (node->camera != nullptr || node->light != nullptr)
            {
                context.result.warnings.push_back("Model preview does not import cameras/lights");
            }

            require(context.result.nodes.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()),
                "Node index exceeds int range");
            const int localIndex = static_cast<int>(context.result.nodes.size());
            context.nodeMapping[index] = localIndex;
            context.result.nodes.push_back(std::move(target));
            for (std::size_t child = 0; child < node->children_count; ++child)
            {
                visit(node->children[child], localIndex);
            }
        };

        for (std::size_t index = 0; index < scene->nodes_count; ++index)
        {
            require(scene->nodes[index] != nullptr && scene->nodes[index]->parent == nullptr,
                "Scene root has a parent or is invalid");
            visit(scene->nodes[index], -1);
        }
    }
}
