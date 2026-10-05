#pragma once

#include <glm/vec3.hpp>

// 射线：方向在构造时归一化，之后pointAt(t)直接用t表示沿射线的距离。
// 零向量或含NaN/无穷大的方向没有归一化意义，构造时抛std::invalid_argument。
struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;

    Ray(const glm::vec3 &origin, const glm::vec3 &direction);

    glm::vec3 pointAt(float t) const;
};
