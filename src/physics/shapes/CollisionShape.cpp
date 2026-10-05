#include "physics/shapes/CollisionShape.h"

#include <glm/geometric.hpp>

#include <cmath>
#include <stdexcept>

namespace
{
    bool isFinite(float value)
    {
        return std::isfinite(value);
    }

    bool isFinite(const glm::vec3 &value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }
}

SphereShape::SphereShape(float radius)
    : radius(radius)
{
    if (!isFinite(radius) || radius <= 0.0f)
    {
        throw std::invalid_argument("SphereShape radius must be positive and finite");
    }
}

BoxShape::BoxShape(const glm::vec3 &halfExtents)
    : halfExtents(halfExtents)
{
    if (!isFinite(halfExtents) || halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
    {
        throw std::invalid_argument("BoxShape half extents must be positive and finite");
    }
}

CapsuleShape::CapsuleShape(float radius, float cylinderHeight)
    : radius(radius), cylinderHeight(cylinderHeight)
{
    if (!isFinite(radius) || radius <= 0.0f)
    {
        throw std::invalid_argument("CapsuleShape radius must be positive and finite");
    }
    if (!isFinite(cylinderHeight) || cylinderHeight < 0.0f)
    {
        throw std::invalid_argument("CapsuleShape cylinder height must not be negative");
    }
}

PlaneShape::PlaneShape(const glm::vec3 &normal, float offset)
    : offset(offset)
{
    if (!isFinite(normal) || !isFinite(offset))
    {
        throw std::invalid_argument("PlaneShape normal and offset must be finite");
    }
    const float length = glm::length(normal);
    if (length <= 0.0f)
    {
        throw std::invalid_argument("PlaneShape normal must not be a zero vector");
    }
    this->normal = normal / length;
}

CollisionShape::CollisionShape(const SphereShape &sphere)
    : data_(sphere)
{
}

CollisionShape::CollisionShape(const BoxShape &box)
    : data_(box)
{
}

CollisionShape::CollisionShape(const CapsuleShape &capsule)
    : data_(capsule)
{
}

CollisionShape::CollisionShape(const PlaneShape &plane)
    : data_(plane)
{
}

Aabb CollisionShape::localAabb() const
{
    if (const auto *sphere = std::get_if<SphereShape>(&data_))
    {
        const glm::vec3 extent(sphere->radius);
        return Aabb(-extent, extent);
    }
    if (const auto *box = std::get_if<BoxShape>(&data_))
    {
        return Aabb(-box->halfExtents, box->halfExtents);
    }
    if (const auto *capsule = std::get_if<CapsuleShape>(&data_))
    {
        const float halfTotal = capsule->cylinderHeight * 0.5f + capsule->radius;
        return Aabb(glm::vec3(-capsule->radius, -halfTotal, -capsule->radius),
            glm::vec3(capsule->radius, halfTotal, capsule->radius));
    }
    throw std::logic_error("PlaneShape has no finite local AABB");
}

bool CollisionShape::isFinite() const
{
    return !std::holds_alternative<PlaneShape>(data_);
}
