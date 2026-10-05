#pragma once

#include <glm/vec3.hpp>

// 纯CPU的金属度/粗糙度工作流参数。颜色均为线性值，不包含Mesh或场景光源。
struct PbrParameters
{
    float metallic = 0.0f;
    float roughness = 0.5f;
    glm::vec3 emission{0.0f}; // 发光只是表面输出，不自动照亮旁边的物体。
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f; // AO仅影响环境近似项，不乘到直射光上。
    float alphaCutoff = 0.5f;
    bool unlit = false;
    void validate() const;
};

// 基础颜色继续使用Material的0号纹理。其余槽位固定映射到纹理单元1..4。
enum class PbrTextureSlot { MetallicRoughness, Normal, Occlusion, Emission };
