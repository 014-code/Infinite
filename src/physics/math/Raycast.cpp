#include "physics/math/Raycast.h"

#include "physics/math/ClosestPoint.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr float PARALLEL_EPSILON = 1e-8f;

    RaycastHit immediateHit(const Ray &ray)
    {
        return RaycastHit{0.0f, ray.origin, -ray.direction};
    }
}

namespace PhysicsRaycast
{
    std::optional<RaycastHit> intersectSphere(const Ray &ray, const glm::vec3 &center, float radius)
    {
        const glm::vec3 offset = ray.origin - center;
        const float c = glm::dot(offset, offset) - radius * radius;
        if (c <= 0.0f)
        {
            return immediateHit(ray);
        }
        // 方向已归一化，二次项系数为1。
        const float b = glm::dot(offset, ray.direction);
        if (b > 0.0f)
        {
            return std::nullopt; // 球心在射线背后且起点在外部。
        }
        const float discriminant = b * b - c;
        if (discriminant < 0.0f)
        {
            return std::nullopt;
        }
        const float t = -b - std::sqrt(discriminant);
        if (t < 0.0f)
        {
            return std::nullopt;
        }
        const glm::vec3 point = ray.pointAt(t);
        return RaycastHit{t, point, (point - center) / radius};
    }

    std::optional<RaycastHit> intersectBox(const Ray &ray, const Aabb &box)
    {
        // 逐轴slab求交；tmin所在的轴决定命中面的法线。
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();
        int hitAxis = -1;
        float hitSign = 1.0f;
        for (int axis = 0; axis < 3; ++axis)
        {
            const float origin = ray.origin[axis];
            const float direction = ray.direction[axis];
            if (std::abs(direction) < PARALLEL_EPSILON)
            {
                if (origin < box.min[axis] || origin > box.max[axis])
                {
                    return std::nullopt;
                }
                continue;
            }
            float t1 = (box.min[axis] - origin) / direction;
            float t2 = (box.max[axis] - origin) / direction;
            float sign = -1.0f; // 从min面进入时法线朝该轴负方向。
            if (t1 > t2)
            {
                std::swap(t1, t2);
                sign = 1.0f;
            }
            if (t1 > tMin)
            {
                tMin = t1;
                hitAxis = axis;
                hitSign = sign;
            }
            tMax = std::min(tMax, t2);
            if (tMin > tMax)
            {
                return std::nullopt;
            }
        }
        if (hitAxis < 0)
        {
            return immediateHit(ray); // 射线与所有slab平行且起点在盒内。
        }
        if (tMax < 0.0f)
        {
            return std::nullopt; // 整个盒在射线背后。
        }
        if (tMin <= 0.0f)
        {
            return immediateHit(ray); // 起点在盒内。
        }
        glm::vec3 normal(0.0f);
        normal[hitAxis] = hitSign;
        return RaycastHit{tMin, ray.pointAt(tMin), normal};
    }

    std::optional<RaycastHit> intersectPlane(const Ray &ray, const glm::vec3 &normal, float offset)
    {
        const float denominator = glm::dot(normal, ray.direction);
        if (std::abs(denominator) < PARALLEL_EPSILON)
        {
            return std::nullopt; // 与平面平行：即使起点在平面上也不报命中，避免整段射线都是解。
        }
        const float t = (offset - glm::dot(normal, ray.origin)) / denominator;
        if (t < 0.0f)
        {
            return std::nullopt;
        }
        return RaycastHit{t, ray.pointAt(t), denominator < 0.0f ? normal : -normal};
    }

    std::optional<RaycastHit> intersectCapsule(const Ray &ray, float radius, float cylinderHeight)
    {
        const float halfHeight = cylinderHeight * 0.5f;
        const glm::vec3 segmentA(0.0f, -halfHeight, 0.0f);
        const glm::vec3 segmentB(0.0f, halfHeight, 0.0f);

        const glm::vec3 onAxis = PhysicsClosestPoint::onSegment(ray.origin, segmentA, segmentB);
        const glm::vec3 fromAxis = ray.origin - onAxis;
        if (glm::dot(fromAxis, fromAxis) <= radius * radius)
        {
            return immediateHit(ray);
        }

        std::optional<RaycastHit> best;

        // 圆柱侧面：在XZ平面解二次方程，再校验Y范围。
        const float a = ray.direction.x * ray.direction.x + ray.direction.z * ray.direction.z;
        if (a > PARALLEL_EPSILON)
        {
            const float b = 2.0f * (ray.origin.x * ray.direction.x + ray.origin.z * ray.direction.z);
            const float c = ray.origin.x * ray.origin.x + ray.origin.z * ray.origin.z - radius * radius;
            const float discriminant = b * b - 4.0f * a * c;
            if (discriminant >= 0.0f)
            {
                const float root = std::sqrt(discriminant);
                for (float t : {(-b - root) / (2.0f * a), (-b + root) / (2.0f * a)})
                {
                    if (t < 0.0f)
                    {
                        continue;
                    }
                    const glm::vec3 point = ray.pointAt(t);
                    if (point.y < -halfHeight || point.y > halfHeight)
                    {
                        continue;
                    }
                    RaycastHit hit{t, point, glm::vec3(point.x, 0.0f, point.z) / radius};
                    if (!best || hit.t < best->t)
                    {
                        best = hit;
                    }
                }
            }
        }

        // 两端半球帽：退化为两次球体求交。
        for (const glm::vec3 &capCenter : {segmentA, segmentB})
        {
            std::optional<RaycastHit> cap = intersectSphere(ray, capCenter, radius);
            if (cap && cap->t > 0.0f && (!best || cap->t < best->t))
            {
                best = cap;
            }
        }
        return best;
    }
}
