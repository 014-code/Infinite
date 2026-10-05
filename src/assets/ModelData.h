#pragma once

#include "MeshData.h"
#include "animation/AnimationClip.h"
#include "animation/Skin.h"
#include "graphics/materials/PbrParameters.h"
#include <array>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec4.hpp>
#include <cstddef>
#include <string>
#include <vector>

// glTF导入的CPU快照，不持有第三方解析器指针或OpenGL对象。
// 索引引用本ModelData内的数组，-1表示没有父节点/纹理；节点按父先子后排列。
struct ModelPrimitiveData
{
    MeshData mesh;
    std::size_t material = 0;
    std::vector<SkinVertex> skinVertices; // 空数组表示静态布局，与mesh顶点逐项对应。
};

struct ModelTextureData
{
    std::size_t image = 0;
    // glTF采样枚举与OpenGL数值一致，解析器仍会校验允许的取值。
    int minFilter = 9987;
    int magFilter = 9729;
    int wrapS = 10497;
    int wrapT = 10497;
};

enum class ModelAlphaMode { Opaque, Mask, Blend };

struct ModelMaterialData
{
    glm::vec4 baseColor{1.0f};
    int texture = -1;
    bool doubleSided = false;
    PbrParameters pbr;
    // 顺序对应MR、normal、occlusion、emission。-1表示没有图片，使用因子/缺省值。
    std::array<int, 4> pbrTextures{{-1, -1, -1, -1}};
    ModelAlphaMode alphaMode = ModelAlphaMode::Opaque;
};

struct ModelNodeData
{
    std::string name;
    int parent = -1;
    glm::vec3 position{0.0f};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 scale{1.0f};
    std::vector<std::size_t> primitives;
    int skin = -1;
};

struct ModelData
{
    bool pbrMaterials = false;
    std::vector<ModelPrimitiveData> primitives;
    std::vector<ModelMaterialData> materials;
    std::vector<ModelTextureData> textures;
    // 图片保留PNG/JPEG编码字节，后续统一交给ImageLoader内存解码。
    std::vector<std::vector<unsigned char>> images;
    std::vector<ModelNodeData> nodes;
    std::vector<AnimationClip> animations;
    std::vector<Skin> skins;
    std::vector<std::string> warnings;
};
