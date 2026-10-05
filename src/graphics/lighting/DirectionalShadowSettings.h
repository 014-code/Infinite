#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

// 主方向光的固定正交覆盖盒。由应用选择范围，不自动跟随相机，也不是级联阴影。
// 阴影只影响主灯直射光，环境光/其他灯不应被一起乘黑。
struct DirectionalShadowSettings
{
    glm::vec3 center{0};
    float halfExtent = 8;
    float distance = 15;
    float nearPlane = .1f;
    float farPlane = 40;
    int resolution = 1024;
    float bias = .001f; // 归一化光源深度单位，过大会产生悬浮阴影。
    void validate() const;
    glm::mat4 lightMatrix(const glm::vec3 &direction) const;
};
