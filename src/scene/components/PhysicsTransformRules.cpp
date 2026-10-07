#include "scene/components/PhysicsTransformRules.h"

#include <cmath>
#include <stdexcept>

namespace
{
    constexpr float UNIT_SCALE_EPSILON = 0.00001f;

    bool isFinite(const glm::vec3 &value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }
}

namespace PhysicsTransformRules
{
    bool hasUnitScale(const Transform &transform) noexcept
    {
        return isFinite(transform.scale) &&
            std::abs(transform.scale.x - 1.0f) <= UNIT_SCALE_EPSILON &&
            std::abs(transform.scale.y - 1.0f) <= UNIT_SCALE_EPSILON &&
            std::abs(transform.scale.z - 1.0f) <= UNIT_SCALE_EPSILON;
    }

    void validate(const Transform &transform)
    {
        if (transform.parent() != nullptr)
        {
            throw std::invalid_argument("Physics Transform must be a root Transform");
        }
        if (!hasUnitScale(transform))
        {
            throw std::invalid_argument("Physics Transform scale must be finite and one");
        }
    }
}
