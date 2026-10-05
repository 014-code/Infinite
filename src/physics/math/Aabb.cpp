#include "physics/math/Aabb.h"

#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    bool isFinite(const glm::vec3 &value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }
}

Aabb::Aabb(const glm::vec3 &min, const glm::vec3 &max)
    : min(min), max(max)
{
    if (!isFinite(min) || !isFinite(max))
    {
        throw std::invalid_argument("Aabb bounds must be finite");
    }
    if (min.x > max.x || min.y > max.y || min.z > max.z)
    {
        throw std::invalid_argument("Aabb min must not exceed max on any axis");
    }
}

Aabb Aabb::fromCenterHalfExtents(const glm::vec3 &center, const glm::vec3 &halfExtents)
{
    if (halfExtents.x < 0.0f || halfExtents.y < 0.0f || halfExtents.z < 0.0f)
    {
        throw std::invalid_argument("Aabb half extents must not be negative");
    }
    return Aabb(center - halfExtents, center + halfExtents);
}

glm::vec3 Aabb::center() const
{
    return (min + max) * 0.5f;
}

glm::vec3 Aabb::halfExtents() const
{
    return (max - min) * 0.5f;
}

bool Aabb::intersects(const Aabb &other) const
{
    return min.x <= other.max.x && max.x >= other.min.x
        && min.y <= other.max.y && max.y >= other.min.y
        && min.z <= other.max.z && max.z >= other.min.z;
}

bool Aabb::contains(const glm::vec3 &point) const
{
    return point.x >= min.x && point.x <= max.x
        && point.y >= min.y && point.y <= max.y
        && point.z >= min.z && point.z <= max.z;
}

bool Aabb::contains(const Aabb &other) const
{
    return contains(other.min) && contains(other.max);
}

Aabb Aabb::expanded(float amount) const
{
    if (!std::isfinite(amount))
    {
        throw std::invalid_argument("Aabb expansion must be finite");
    }
    const glm::vec3 delta(amount);
    return Aabb(min - delta, max + delta);
}

Aabb Aabb::united(const Aabb &other) const
{
    return Aabb(glm::vec3(std::min(min.x, other.min.x), std::min(min.y, other.min.y),
            std::min(min.z, other.min.z)),
        glm::vec3(std::max(max.x, other.max.x), std::max(max.y, other.max.y),
            std::max(max.z, other.max.z)));
}

Aabb Aabb::transformed(const glm::mat4 &matrix) const
{
    // 从min出发枚举8个角点，逐位选择min或max分量。
    glm::vec3 resultMin(0.0f);
    glm::vec3 resultMax(0.0f);
    for (int corner = 0; corner < 8; ++corner)
    {
        const glm::vec3 local(
            (corner & 1) ? max.x : min.x,
            (corner & 2) ? max.y : min.y,
            (corner & 4) ? max.z : min.z);
        const glm::vec3 world(matrix * glm::vec4(local, 1.0f));
        if (!isFinite(world))
        {
            throw std::invalid_argument("Aabb transform produced a non-finite corner");
        }
        if (corner == 0)
        {
            resultMin = world;
            resultMax = world;
        }
        else
        {
            resultMin = glm::vec3(std::min(resultMin.x, world.x), std::min(resultMin.y, world.y),
                std::min(resultMin.z, world.z));
            resultMax = glm::vec3(std::max(resultMax.x, world.x), std::max(resultMax.y, world.y),
                std::max(resultMax.z, world.z));
        }
    }
    return Aabb(resultMin, resultMax);
}
