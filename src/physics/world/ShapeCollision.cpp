#include "physics/world/ShapeCollision.h"

#include "physics/math/ClosestPoint.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    constexpr float CONTACT_EPSILON = 1e-6f;

    // 平面在形状后方时的接触：取形状沿-法线的最深支撑点，穿透深度即该点越过平面的距离。
    std::optional<ShapeContact> planeVsShape(const PlaneShape &plane, const glm::vec3 &planePosition,
        const glm::quat &planeRotation, const CollisionShape &shape, const glm::vec3 &shapePosition,
        const glm::quat &shapeRotation)
    {
        glm::vec3 normal(0.0f);
        float offset = 0.0f;
        ShapeCollision::planeToWorld(plane, planePosition, planeRotation, normal, offset);

        const glm::vec3 deepest = ShapeCollision::supportWorld(shape, shapePosition, shapeRotation, -normal);
        const float signedDistance = glm::dot(normal, deepest) - offset;
        if (signedDistance > CONTACT_EPSILON)
        {
            return std::nullopt; // 形状完全在平面前侧。
        }
        return ShapeContact{deepest, normal, std::max(0.0f, -signedDistance)};
    }

    std::optional<ShapeContact> sphereVsSphere(const glm::vec3 &positionA, float radiusA,
        const glm::vec3 &positionB, float radiusB)
    {
        const glm::vec3 delta = positionB - positionA;
        const float distanceSquared = glm::dot(delta, delta);
        const float radiusSum = radiusA + radiusB;
        if (distanceSquared > radiusSum * radiusSum)
        {
            return std::nullopt;
        }
        const float distance = std::sqrt(distanceSquared);
        // 完全重合时法线无定义，固定取+Y，避免返回NaN。
        const glm::vec3 normal = distance > CONTACT_EPSILON ? delta / distance : glm::vec3(0.0f, 1.0f, 0.0f);
        const float penetration = radiusSum - distance;
        return ShapeContact{positionA + normal * (radiusA - penetration * 0.5f), normal, penetration};
    }

    // 球体与可旋转盒体：把球心变换到盒体局部空间后与半边长盒求最近点。
    // 返回的normal从球指向盒体，point为盒体表面最近点的世界坐标。
    std::optional<ShapeContact> sphereVsBox(const glm::vec3 &spherePosition, float radius,
        const glm::vec3 &boxPosition, const glm::quat &boxRotation, const glm::vec3 &halfExtents)
    {
        const glm::quat rotation = ShapeCollision::normalizedRotation(boxRotation);
        const glm::mat4 worldMatrix = ShapeCollision::composeTransform(boxPosition, rotation);
        const glm::mat4 inverseWorld = glm::inverse(worldMatrix);

        const glm::vec3 localCenter(inverseWorld * glm::vec4(spherePosition, 1.0f));
        const Aabb localBox(-halfExtents, halfExtents);
        const glm::vec3 closest = PhysicsClosestPoint::onAabb(localCenter, localBox);
        const glm::vec3 fromSurface = localCenter - closest;
        const float distance = glm::length(fromSurface);
        if (distance > radius)
        {
            return std::nullopt;
        }

        glm::vec3 localNormal(0.0f);
        float penetration = 0.0f;
        if (distance > CONTACT_EPSILON)
        {
            // 球心在盒外：沿盒体表面外法线的反方向推开盒体即可分离。
            localNormal = -fromSurface / distance;
            penetration = radius - distance;
        }
        else
        {
            // 球心在盒内：选最近的面对，法线取该面外法线的反方向。
            int axis = 0;
            float faceDistance = halfExtents.x - std::abs(localCenter.x);
            for (int candidate = 1; candidate < 3; ++candidate)
            {
                const float candidateDistance = halfExtents[candidate] - std::abs(localCenter[candidate]);
                if (candidateDistance < faceDistance)
                {
                    faceDistance = candidateDistance;
                    axis = candidate;
                }
            }
            const float sign = localCenter[axis] >= 0.0f ? 1.0f : -1.0f;
            localNormal[axis] = -sign;
            penetration = radius + faceDistance;
        }

        const glm::vec3 normal = rotation * localNormal;
        const glm::vec3 worldClosest(worldMatrix * glm::vec4(closest, 1.0f));
        return ShapeContact{worldClosest, normal, penetration};
    }

    // 胶囊与可旋转盒体：把胶囊轴线变换到盒体局部空间，求轴线线段到AABB的最近点，
    // 再按"该点处的球体"处理。线段到凸体的距离是凸函数，用三分搜索即可稳定收敛。
    // 返回的normal从胶囊指向盒体，point为盒体表面最近点的世界坐标。
    std::optional<ShapeContact> capsuleVsBox(const glm::vec3 &capsulePosition, const glm::quat &capsuleRotation,
        float radius, float cylinderHeight, const glm::vec3 &boxPosition, const glm::quat &boxRotation,
        const glm::vec3 &halfExtents)
    {
        const glm::quat boxOrientation = ShapeCollision::normalizedRotation(boxRotation);
        const glm::mat4 boxWorld = ShapeCollision::composeTransform(boxPosition, boxOrientation);
        const glm::mat4 inverseBox = glm::inverse(boxWorld);

        const glm::vec3 axisWorld = ShapeCollision::normalizedRotation(capsuleRotation) * glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 axisLocal(glm::inverse(glm::mat4_cast(boxOrientation)) * glm::vec4(axisWorld, 0.0f));
        const glm::vec3 centerLocal(inverseBox * glm::vec4(capsulePosition, 1.0f));
        const float halfHeight = cylinderHeight * 0.5f;
        const Aabb localBox(-halfExtents, halfExtents);

        const auto distanceAt = [&](float along)
        {
            const glm::vec3 axisPoint = centerLocal + axisLocal * along;
            return glm::length(axisPoint - PhysicsClosestPoint::onAabb(axisPoint, localBox));
        };
        float low = -halfHeight;
        float high = halfHeight;
        for (int iteration = 0; iteration < 24; ++iteration)
        {
            const float third = (high - low) / 3.0f;
            const float left = low + third;
            const float right = high - third;
            if (distanceAt(left) < distanceAt(right))
            {
                high = right;
            }
            else
            {
                low = left;
            }
        }
        const glm::vec3 axisPointLocal = centerLocal + axisLocal * ((low + high) * 0.5f);
        const glm::vec3 closestLocal = PhysicsClosestPoint::onAabb(axisPointLocal, localBox);
        const glm::vec3 fromSurface = axisPointLocal - closestLocal;
        const float distance = glm::length(fromSurface);

        // localPushOut把胶囊推出盒体（盒体局部空间）。
        glm::vec3 localPushOut(0.0f);
        float penetration = 0.0f;
        if (distance > CONTACT_EPSILON)
        {
            if (distance > radius)
            {
                return std::nullopt;
            }
            localPushOut = fromSurface / distance;
            penetration = radius - distance;
        }
        else
        {
            // 轴线落在盒体内部：沿最近的一面推出。
            int axis = 0;
            float faceDistance = halfExtents.x - std::abs(axisPointLocal.x);
            for (int candidate = 1; candidate < 3; ++candidate)
            {
                const float candidateDistance = halfExtents[candidate] - std::abs(axisPointLocal[candidate]);
                if (candidateDistance < faceDistance)
                {
                    faceDistance = candidateDistance;
                    axis = candidate;
                }
            }
            localPushOut[axis] = axisPointLocal[axis] >= 0.0f ? 1.0f : -1.0f;
            penetration = radius + faceDistance;
        }

        const glm::vec3 pushOutWorld = boxOrientation * localPushOut;
        const glm::vec3 pointWorld(boxWorld * glm::vec4(closestLocal, 1.0f));
        // 约定：A为胶囊，B为盒体，normal从胶囊指向盒体（与推出方向相反）。
        return ShapeContact{pointWorld, -pushOutWorld, penetration};
    }
}

namespace ShapeCollision
{
    glm::quat normalizedRotation(const glm::quat &rotation)
    {
        const float length = glm::length(rotation);
        if (!std::isfinite(length) || length <= 0.0f)
        {
            throw std::invalid_argument("Physics rotation must be a finite non-zero quaternion");
        }
        return rotation / length;
    }

    glm::vec3 separationDirection(const ShapeContact &contact, bool otherIsPlane)
    {
        return otherIsPlane ? contact.normal : -contact.normal;
    }

    glm::mat4 composeTransform(const glm::vec3 &position, const glm::quat &rotation)
    {
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        {
            throw std::invalid_argument("Physics position must be finite");
        }
        return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(normalizedRotation(rotation));
    }

    void planeToWorld(const PlaneShape &plane, const glm::vec3 &position, const glm::quat &rotation,
        glm::vec3 &outNormal, float &outOffset)
    {
        const glm::quat orientation = ShapeCollision::normalizedRotation(rotation);
        outNormal = orientation * plane.normal;
        outOffset = plane.offset + glm::dot(outNormal, position);
    }

    glm::vec3 supportWorld(const CollisionShape &shape, const glm::vec3 &position, const glm::quat &rotation,
        const glm::vec3 &direction)
    {
        const float length = glm::length(direction);
        if (!std::isfinite(length) || length <= CONTACT_EPSILON)
        {
            throw std::invalid_argument("Support direction must be a finite non-zero vector");
        }
        const glm::vec3 unit = direction / length;
        const glm::quat orientation = ShapeCollision::normalizedRotation(rotation);

        if (shape.holds<SphereShape>())
        {
            return position + unit * shape.get<SphereShape>().radius;
        }
        if (shape.holds<BoxShape>())
        {
            const glm::vec3 localDirection = glm::inverse(orientation) * unit;
            const glm::vec3 &halfExtents = shape.get<BoxShape>().halfExtents;
            const glm::vec3 localSupport(
                localDirection.x >= 0.0f ? halfExtents.x : -halfExtents.x,
                localDirection.y >= 0.0f ? halfExtents.y : -halfExtents.y,
                localDirection.z >= 0.0f ? halfExtents.z : -halfExtents.z);
            return position + orientation * localSupport;
        }
        if (shape.holds<CapsuleShape>())
        {
            const CapsuleShape &capsule = shape.get<CapsuleShape>();
            const glm::vec3 axis = orientation * glm::vec3(0.0f, 1.0f, 0.0f);
            const float along = glm::dot(axis, unit) >= 0.0f ? 1.0f : -1.0f;
            return position + axis * (capsule.cylinderHeight * 0.5f * along) + unit * capsule.radius;
        }
        throw std::invalid_argument("PlaneShape has no support point");
    }

    Aabb worldAabb(const CollisionShape &shape, const glm::vec3 &position, const glm::quat &rotation)
    {
        return shape.localAabb().transformed(composeTransform(position, rotation));
    }

    std::optional<ShapeContact> collide(const CollisionShape &shapeA, const glm::vec3 &positionA,
        const glm::quat &rotationA, const CollisionShape &shapeB, const glm::vec3 &positionB,
        const glm::quat &rotationB)
    {
        const bool planeA = shapeA.holds<PlaneShape>();
        const bool planeB = shapeB.holds<PlaneShape>();
        if (planeA && planeB)
        {
            return std::nullopt; // 平面与平面没有有界接触区域。
        }
        if (planeA)
        {
            return planeVsShape(shapeA.get<PlaneShape>(), positionA, rotationA, shapeB, positionB, rotationB);
        }
        if (planeB)
        {
            return planeVsShape(shapeB.get<PlaneShape>(), positionB, rotationB, shapeA, positionA, rotationA);
        }

        const bool sphereA = shapeA.holds<SphereShape>();
        const bool sphereB = shapeB.holds<SphereShape>();
        const bool boxA = shapeA.holds<BoxShape>();
        const bool boxB = shapeB.holds<BoxShape>();
        const bool capsuleA = shapeA.holds<CapsuleShape>();
        const bool capsuleB = shapeB.holds<CapsuleShape>();

        if (sphereA && sphereB)
        {
            return sphereVsSphere(positionA, shapeA.get<SphereShape>().radius, positionB,
                shapeB.get<SphereShape>().radius);
        }
        if (sphereA && boxB)
        {
            return sphereVsBox(positionA, shapeA.get<SphereShape>().radius, positionB, rotationB,
                shapeB.get<BoxShape>().halfExtents);
        }
        if (boxA && sphereB)
        {
            std::optional<ShapeContact> contact = sphereVsBox(positionB, shapeB.get<SphereShape>().radius,
                positionA, rotationA, shapeA.get<BoxShape>().halfExtents);
            if (contact)
            {
                contact->normal = -contact->normal; // 交换顺序后法线方向随之取反。
            }
            return contact;
        }
        if (capsuleA && boxB)
        {
            const CapsuleShape &capsule = shapeA.get<CapsuleShape>();
            return capsuleVsBox(positionA, rotationA, capsule.radius, capsule.cylinderHeight, positionB, rotationB,
                shapeB.get<BoxShape>().halfExtents);
        }
        if (boxA && capsuleB)
        {
            const CapsuleShape &capsule = shapeB.get<CapsuleShape>();
            std::optional<ShapeContact> contact = capsuleVsBox(positionB, rotationB, capsule.radius,
                capsule.cylinderHeight, positionA, rotationA, shapeA.get<BoxShape>().halfExtents);
            if (contact)
            {
                contact->normal = -contact->normal;
            }
            return contact;
        }
        return std::nullopt; // 其余组合首版不支持，不返回近似结果。
    }

    bool intersects(const CollisionShape &shapeA, const glm::vec3 &positionA, const glm::quat &rotationA,
        const CollisionShape &shapeB, const glm::vec3 &positionB, const glm::quat &rotationB)
    {
        return collide(shapeA, positionA, rotationA, shapeB, positionB, rotationB).has_value();
    }
}
