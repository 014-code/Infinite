#include "physics/math/Ray.h"

#include <glm/geometric.hpp>

#include <cmath>
#include <stdexcept>

Ray::Ray(const glm::vec3 &origin, const glm::vec3 &direction)
    : origin(origin)
{
    const bool finiteOrigin = std::isfinite(origin.x) && std::isfinite(origin.y) && std::isfinite(origin.z);
    const float length = glm::length(direction);
    if (!finiteOrigin || !std::isfinite(length) || length <= 0.0f)
    {
        throw std::invalid_argument("Ray requires a finite origin and a non-zero direction");
    }
    this->direction = direction / length;
}

glm::vec3 Ray::pointAt(float t) const
{
    return origin + direction * t;
}
