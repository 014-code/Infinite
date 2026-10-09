#include "PhysicsQueries.h"

#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    // 射线与包围盒的快速排除；不相交的体不进入精确求交。
    bool rayHitsAabb(const Ray &ray, const Aabb &box)
    {
        return PhysicsRaycast::intersectBox(ray, box).has_value();
    }
}

std::optional<PhysicsWorld::RaycastResult> PhysicsQueries::raycast(const PhysicsWorld &world,
    const Ray &ray, float maxDistance, std::uint32_t queryMask)
{
    if (!std::isfinite(maxDistance) || maxDistance < 0.0f)
    {
        throw std::invalid_argument("Raycast maxDistance must be finite and non-negative");
    }
    std::optional<PhysicsWorld::RaycastResult> best;
    for (const auto &body : world.bodies_)
    {
        if (!filterMatchesQuery(body->filter, queryMask))
        {
            continue;
        }
        // 有限形状先用包围盒排除；平面没有包围盒，直接进入精确求交。
        if (body->worldAabb && !rayHitsAabb(ray, *body->worldAabb))
        {
            continue;
        }

        std::optional<RaycastHit> hit;
        if (body->shape.holds<PlaneShape>())
        {
            glm::vec3 normal(0.0f);
            float offset = 0.0f;
            ShapeCollision::planeToWorld(body->shape.get<PlaneShape>(), body->position, body->rotation,
                normal, offset);
            hit = PhysicsRaycast::intersectPlane(ray, normal, offset);
        }
        else
        {
            // 碰撞体不含缩放，因此把射线变换到局部空间后t与世界空间一致。
            const glm::mat4 inverseWorld = glm::inverse(ShapeCollision::composeTransform(
                body->position, body->rotation));
            const glm::vec3 localOrigin(inverseWorld * glm::vec4(ray.origin, 1.0f));
            const glm::vec3 localDirection(inverseWorld * glm::vec4(ray.direction, 0.0f));
            const Ray localRay(localOrigin, localDirection);

            if (body->shape.holds<SphereShape>())
            {
                hit = PhysicsRaycast::intersectSphere(localRay, glm::vec3(0.0f),
                    body->shape.get<SphereShape>().radius);
            }
            else if (body->shape.holds<BoxShape>())
            {
                const glm::vec3 &halfExtents = body->shape.get<BoxShape>().halfExtents;
                hit = PhysicsRaycast::intersectBox(localRay, Aabb(-halfExtents, halfExtents));
            }
            else if (body->shape.holds<CapsuleShape>())
            {
                const CapsuleShape &capsule = body->shape.get<CapsuleShape>();
                hit = PhysicsRaycast::intersectCapsule(localRay, capsule.radius, capsule.cylinderHeight);
            }

            // 命中点与法线需要回到世界空间；t在刚体变换下不变。
            if (hit)
            {
                const glm::mat4 worldMatrix = ShapeCollision::composeTransform(body->position,
                    body->rotation);
                hit->point = glm::vec3(worldMatrix * glm::vec4(hit->point, 1.0f));
                hit->normal = body->rotation * hit->normal;
            }
        }

        if (hit && hit->t <= maxDistance && (!best || hit->t < best->hit.t))
        {
            best = PhysicsWorld::RaycastResult{body->id, *hit, body->dynamic};
        }
    }
    return best;
}

std::vector<PhysicsWorld::OverlapResult> PhysicsQueries::overlapShape(const PhysicsWorld &world,
    const CollisionShape &shape, const glm::vec3 &position, const glm::quat &rotation,
    std::uint32_t queryMask)
{
    // 查询形状是平面时没有有限范围，且平面-平面组合不受支持，直接报错而不是返回空结果。
    if (shape.holds<PlaneShape>())
    {
        throw std::invalid_argument("overlapShape does not accept a PlaneShape query");
    }
    const Aabb queryAabb = shape.localAabb().transformed(
        ShapeCollision::composeTransform(position, rotation));

    std::vector<PhysicsWorld::OverlapResult> results;
    for (const auto &body : world.bodies_)
    {
        if (!filterMatchesQuery(body->filter, queryMask))
        {
            continue;
        }
        if (body->worldAabb && !queryAabb.intersects(*body->worldAabb))
        {
            continue;
        }
        const std::optional<ShapeContact> contact = ShapeCollision::collide(shape, position, rotation,
            body->shape, body->position, body->rotation);
        if (contact)
        {
            results.push_back({body->id, *contact,
                ShapeCollision::separationDirection(*contact, body->shape.holds<PlaneShape>()),
                body->dynamic});
        }
    }
    return results;
}

void PhysicsQueries::collectSweepCandidates(const PhysicsWorld &world,
    std::vector<std::pair<std::size_t, std::size_t>> &candidates)
{
    // 平面没有有限包围盒，不参与扫掠剪枝；它们由接触收集单独与动态体配对。
    std::vector<std::size_t> sorted;
    sorted.reserve(world.bodies_.size());
    for (std::size_t index = 0; index < world.bodies_.size(); ++index)
    {
        if (world.bodies_[index]->worldAabb)
        {
            sorted.push_back(index);
        }
    }
    std::sort(sorted.begin(), sorted.end(),
        [&world](std::size_t left, std::size_t right)
        {
            return world.bodies_[left]->worldAabb->min.x < world.bodies_[right]->worldAabb->min.x;
        });

    for (std::size_t i = 0; i < sorted.size(); ++i)
    {
        const Aabb &current = *world.bodies_[sorted[i]]->worldAabb;
        for (std::size_t j = i + 1; j < sorted.size(); ++j)
        {
            const auto &candidate = *world.bodies_[sorted[j]];
            if (candidate.worldAabb->min.x > current.max.x)
            {
                break; // 后续体的min.x只会更大，可以结束本轮的扫描。
            }
            if (!current.intersects(*candidate.worldAabb))
            {
                continue;
            }
            if (!filtersInteract(world.bodies_[sorted[i]]->filter, candidate.filter))
            {
                continue; // 层/掩码不允许这一对，连候选对也不产生。
            }
            candidates.emplace_back(sorted[i], sorted[j]);
        }
    }
}

std::vector<std::pair<PhysicsBodyId, PhysicsBodyId>> PhysicsQueries::broadphasePairs(
    const PhysicsWorld &world)
{
    std::vector<std::pair<std::size_t, std::size_t>> candidates;
    collectSweepCandidates(world, candidates);

    std::vector<std::pair<PhysicsBodyId, PhysicsBodyId>> pairs;
    pairs.reserve(candidates.size());
    for (const auto &candidate : candidates)
    {
        pairs.emplace_back(world.bodies_[candidate.first]->id, world.bodies_[candidate.second]->id);
    }
    return pairs;
}
