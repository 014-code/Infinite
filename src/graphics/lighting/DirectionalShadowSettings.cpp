#include "DirectionalShadowSettings.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <stdexcept>

void DirectionalShadowSettings::validate() const
{
    for (float value : {center.x,center.y,center.z,halfExtent,distance,nearPlane,farPlane,bias})
    { if (!std::isfinite(value)) { throw std::invalid_argument("Non-finite directional shadow settings"); } }
    if (halfExtent <= 0 || distance <= 0 || nearPlane <= 0 || farPlane <= nearPlane ||
        resolution < 16 || resolution > 8192 || bias < 0 || bias > .1f)
    { throw std::invalid_argument("Invalid directional shadow range/resolution/bias"); }
}

glm::mat4 DirectionalShadowSettings::lightMatrix(const glm::vec3 &direction) const
{
    validate();
    const float length = glm::length(direction);
    if (!std::isfinite(length) || length <= 1e-8f) { throw std::invalid_argument("Invalid shadow light direction"); }
    const auto forward = direction / length;
    // 光线接近竖直时改用Z轴作up，避免lookAt的叉积退化。
    const auto up = std::abs(forward.y) > .99f ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
    const auto result = glm::ortho(-halfExtent,halfExtent,-halfExtent,halfExtent,nearPlane,farPlane) *
        glm::lookAt(center - forward * distance, center, up);
    for (int c=0;c<4;++c) { for (int r=0;r<4;++r)
    { if (!std::isfinite(result[c][r])) { throw std::invalid_argument("Shadow matrix is not finite"); } } }
    return result;
}
