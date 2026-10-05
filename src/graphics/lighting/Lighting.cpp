#include "graphics/lighting/Lighting.h"

#include <cmath>
#include <algorithm>
#include <glm/mat3x3.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <stdexcept>

void validateDirectionalLight(const DirectionalLight &light)
{
    for (const float value : {light.direction.x, light.direction.y, light.direction.z,
        light.color.r, light.color.g, light.color.b, light.intensity,
        light.ambient.r, light.ambient.g, light.ambient.b})
    {
        if (!std::isfinite(value))
        {
            throw std::invalid_argument("Directional light values must be finite");
        }
    }
    if (light.direction == glm::vec3(0.0f))
    {
        throw std::invalid_argument("Directional light direction must not be zero");
    }
    if (light.intensity < 0.0f || light.color.r < 0.0f || light.color.g < 0.0f ||
        light.color.b < 0.0f || light.ambient.r < 0.0f || light.ambient.g < 0.0f ||
        light.ambient.b < 0.0f)
    {
        throw std::invalid_argument("Directional light values must not be negative");
    }
}

glm::vec3 normalizedLightDirection(const DirectionalLight &light)
{
    validateDirectionalLight(light);
    const float largest = std::max({std::abs(light.direction.x), std::abs(light.direction.y),
        std::abs(light.direction.z)});
    return glm::normalize(light.direction / largest);
}

glm::mat3 lightingNormalMatrix(const glm::mat4 &world)
{
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            if (!std::isfinite(world[column][row]))
            {
                throw std::invalid_argument("Lit model matrix must be finite");
            }
        }
    }
    // 中间用double计算，避免均匀的小缩放在float行列式计算时下溢。
    const glm::dmat3 basis(world);
    if (glm::determinant(basis) == 0.0)
    {
        throw std::invalid_argument("Lit model matrix is singular (zero scale)");
    }
    const glm::mat3 normal(glm::transpose(glm::inverse(basis)));
    for (int column = 0; column < 3; ++column)
    {
        for (int row = 0; row < 3; ++row)
        {
            if (!std::isfinite(normal[column][row]))
            {
                throw std::invalid_argument("Lit normal matrix exceeds float range");
            }
        }
    }
    return normal;
}
