#include "SceneLighting.h"

#include <glm/gtc/constants.hpp>
#include <cmath>
#include <stdexcept>
#include <string>

namespace
{
    void checkPoint(const PointLight &light, const std::string &label)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            // 防止GPU上距离平方溢出；此范围远大于当前示例的米制场景。
            if (!std::isfinite(light.position[axis]) || std::abs(light.position[axis]) > 1e10f)
            {
                throw std::invalid_argument(label + " position must be finite and within +/-1e10");
            }
            if (!std::isfinite(light.color[axis]) || light.color[axis] < 0 || light.color[axis] > 1e6f)
            {
                throw std::invalid_argument(label + " color must be in [0, 1e6]");
            }
        }
        if (!std::isfinite(light.intensity) || light.intensity < 0 || light.intensity > 1e6f ||
            !std::isfinite(light.range) || light.range < 1e-4f || light.range > 1e6f)
        {
            throw std::invalid_argument(label + " requires intensity in [0, 1e6] and range in [1e-4, 1e6]");
        }
    }
}

SceneLighting SceneLighting::fromDirectionalLight(const DirectionalLight &light)
{
    SceneLighting result;
    result.mainLight_ = light;
    return result;
}

void SceneLighting::validate() const
{
    if (additionalDirectionalLights.size() >= maxDirectionalLights ||
        pointLights.size() > maxPointLights || spotLights.size() > maxSpotLights)
    {
        throw std::invalid_argument("SceneLighting exceeds 2 directional / 8 point / 4 spot lights");
    }
    validateDirectionalLight(mainLight_);
    for (const auto &light : additionalDirectionalLights)
    {
        // 复用方向向量稳定归一化的校验；附加方向光不具有环境光项。
        validateDirectionalLight({light.direction, light.color, light.intensity, glm::vec3(0)});
    }
    for (std::size_t i = 0; i < pointLights.size(); ++i)
    {
        checkPoint(pointLights[i], "PointLight[" + std::to_string(i) + "]");
    }
    for (std::size_t i = 0; i < spotLights.size(); ++i)
    {
        const auto &light = spotLights[i];
        const auto label = "SpotLight[" + std::to_string(i) + "]";
        checkPoint(light, label);
        try { normalizedLightDirection({light.direction}); }
        catch (const std::invalid_argument &error)
        {
            throw std::invalid_argument(label + ": " + error.what());
        }
        if (!std::isfinite(light.innerAngle) || !std::isfinite(light.outerAngle) ||
            light.innerAngle < 0 || light.innerAngle >= light.outerAngle ||
            light.outerAngle >= glm::half_pi<float>() ||
            std::cos(light.innerAngle) - std::cos(light.outerAngle) < 1e-5f)
        {
            throw std::invalid_argument(label + " requires 0 <= inner < outer < pi/2 with a nonzero cosine interval");
        }
    }
}
