#pragma once

#include "physics/math/Aabb.h"

#include <glm/vec3.hpp>

// 最近点计算：供胶囊内部判定、穿透深度估算等narrowphase逻辑共用。
namespace PhysicsClosestPoint
{
    // 线段退化（a==b）时返回该点本身。
    glm::vec3 onSegment(const glm::vec3 &point, const glm::vec3 &a, const glm::vec3 &b);

    // 点在盒内时返回点本身，否则返回各轴夹紧后的表面点。
    glm::vec3 onAabb(const glm::vec3 &point, const Aabb &box);
}
