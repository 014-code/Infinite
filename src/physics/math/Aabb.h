#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

// 轴对齐包围盒：min/max为各轴边界。不表达空盒；构造时要求min逐分量不大于max
// 且数值有限，非法输入抛std::invalid_argument。
class Aabb
{
public:
    glm::vec3 min;
    glm::vec3 max;

    Aabb(const glm::vec3 &min, const glm::vec3 &max);
    static Aabb fromCenterHalfExtents(const glm::vec3 &center, const glm::vec3 &halfExtents);

    glm::vec3 center() const;
    glm::vec3 halfExtents() const;

    // 边界相切视为相交；broadphase需要接触面相邻的盒也进入候选对，不能漏掉恰好接触的物体。
    bool intersects(const Aabb &other) const;
    // 边界上的点/盒视为被包含。
    bool contains(const glm::vec3 &point) const;
    bool contains(const Aabb &other) const;

    // 各轴向外扩展amount；负amount会收缩，收缩到翻转视为非法输入。
    Aabb expanded(float amount) const;
    Aabb united(const Aabb &other) const;

    // 按8个角点变换后重新取包围盒；旋转后的结果是放大的AABB，不是OBB。
    Aabb transformed(const glm::mat4 &matrix) const;
};
