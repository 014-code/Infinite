#pragma once

#include "assets/ModelData.h"
#include <glm/gtc/quaternion.hpp>
#include <cstddef>
#include <memory>
#include <filesystem>
#include <string>
#include <vector>

class Material;
class Mesh;
class ResourceManager;
struct ModelTextureSet;

// Model保存共享GPU资源和只读动画片段。它可以被多个Scene实例共享，
// 但Model中的节点Transform只是初始数据，不会被某个实例直接修改。
class Model final
{
public:
    struct Primitive
    {
        std::shared_ptr<const Mesh> mesh;
        std::shared_ptr<const Material> material;
    };

    struct Node
    {
        std::string name;
        int parent = -1;
        glm::vec3 position{0.0f};
        glm::quat rotation{1, 0, 0, 0};
        glm::vec3 scale{1.0f};
        std::vector<std::size_t> primitives;
        int skin = -1;
    };

    const std::filesystem::path &sourcePath() const noexcept;
    const std::vector<Primitive> &primitives() const noexcept;
    const std::vector<Node> &nodes() const noexcept;
    const std::vector<AnimationClip> &animations() const noexcept { return animations_; }
    const std::vector<Skin> &skins() const noexcept { return skins_; }

private:
    friend class ResourceManager;
    Model(const std::filesystem::path &sourcePath, const ModelData &data,
        std::shared_ptr<class Shader> shader,
        const ModelTextureSet &textures);

    std::filesystem::path sourcePath_;
    std::vector<Primitive> primitives_;
    std::vector<Node> nodes_;
    std::vector<AnimationClip> animations_;
    std::vector<Skin> skins_;
};
