#pragma once

#include "physics/world/PhysicsWorld.h"

#include <cstddef>
#include <utility>
#include <vector>

// PhysicsQueries实现PhysicsWorld的只读空间查询。
//
// PhysicsWorld仍然保留对外的查询接口；本类只是内部实现模块，通过友元访问
// 已经缓存好的Body包围盒和过滤数据，确保查询与物理步使用同一套数据。
class PhysicsQueries final
{
public:
    static std::optional<PhysicsWorld::RaycastResult> raycast(const PhysicsWorld &world,
        const Ray &ray, float maxDistance, std::uint32_t queryMask);
    static std::vector<PhysicsWorld::OverlapResult> overlapShape(const PhysicsWorld &world,
        const CollisionShape &shape, const glm::vec3 &position, const glm::quat &rotation,
        std::uint32_t queryMask);

    // 只返回Body下标，供PhysicsWorld的接触收集复用；下标不会跨越本次查询保存。
    static void collectSweepCandidates(const PhysicsWorld &world,
        std::vector<std::pair<std::size_t, std::size_t>> &candidates);
    static std::vector<std::pair<PhysicsBodyId, PhysicsBodyId>> broadphasePairs(
        const PhysicsWorld &world);
};
