#include "physics/math/ClosestPoint.h"

#include <glm/geometric.hpp>

#include <algorithm>

namespace PhysicsClosestPoint
{
    glm::vec3 onSegment(const glm::vec3 &point, const glm::vec3 &a, const glm::vec3 &b)
    {
        const glm::vec3 ab = b - a;
        const float lengthSquared = glm::dot(ab, ab);
        if (lengthSquared <= 0.0f)
        {
            return a;
        }
        const float t = std::clamp(glm::dot(point - a, ab) / lengthSquared, 0.0f, 1.0f);
        return a + ab * t;
    }

    glm::vec3 onAabb(const glm::vec3 &point, const Aabb &box)
    {
        return glm::vec3(
            std::clamp(point.x, box.min.x, box.max.x),
            std::clamp(point.y, box.min.y, box.max.y),
            std::clamp(point.z, box.min.z, box.max.z));
    }
}
