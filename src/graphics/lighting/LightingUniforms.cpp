#include "LightingUniforms.h"
#include "graphics/resources/Shader.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace
{
    glm::vec3 normalizeDirection(const glm::vec3 &direction)
    {
        return normalizedLightDirection({direction});
    }

    void uploadDirection(const Shader &shader, const std::string &prefix,
        const glm::vec3 &direction, const glm::vec3 &color, float intensity)
    {
        shader.setVec3(prefix + ".direction", direction);
        shader.setVec3(prefix + ".color", color);
        shader.setFloat(prefix + ".intensity", intensity);
    }

    void uploadPoint(const Shader &shader, const std::string &prefix, const PointLight &light)
    {
        shader.setVec3(prefix + ".position", light.position);
        shader.setVec3(prefix + ".color", light.color);
        shader.setFloat(prefix + ".intensity", light.intensity);
        shader.setFloat(prefix + ".range", light.range);
    }
}

LightingUniforms::LightingUniforms(const SceneLighting &lighting) : lighting_(lighting)
{
    lighting_.validate();
    lighting_.mainLight().direction = normalizeDirection(lighting_.mainLight().direction);
    for (auto &light : lighting_.additionalDirectionalLights) { light.direction = normalizeDirection(light.direction); }
    for (auto &light : lighting_.spotLights) { light.direction = normalizeDirection(light.direction); }
}

void LightingUniforms::validateShader(const Shader &shader) const
{
    const bool multiLight = shader.hasUniform("directionalLightCount");
    const bool legacyLit = shader.hasUniform("lightDirection") || shader.hasUniform("lightColor") ||
        shader.hasUniform("lightIntensity");
    if (!multiLight && legacyLit && (!lighting_.additionalDirectionalLights.empty() ||
        !lighting_.pointLights.empty() || !lighting_.spotLights.empty()))
    {
        throw std::invalid_argument("Single-light Shader cannot render SceneLighting with additional lights; use a scene-lighting Shader");
    }
}

void LightingUniforms::upload(const Shader &shader) const
{
    const auto &main = lighting_.mainLight();
    shader.setVec3("ambientLight", lighting_.ambient());
    if (!shader.hasUniform("directionalLightCount"))
    {
        // 保留旧GLSL教学示例的接口；无光照Shader会自然忽略这些uniform。
        shader.setVec3("lightDirection", main.direction);
        shader.setVec3("lightColor", main.color);
        shader.setFloat("lightIntensity", main.intensity);
        return;
    }
    shader.setInt("directionalLightCount", static_cast<int>(lighting_.additionalDirectionalLights.size() + 1));
    uploadDirection(shader, "directionalLights[0]", main.direction, main.color, main.intensity);
    for (std::size_t i = 0; i < lighting_.additionalDirectionalLights.size(); ++i)
    {
        const auto &light = lighting_.additionalDirectionalLights[i];
        uploadDirection(shader, "directionalLights[" + std::to_string(i + 1) + "]", light.direction, light.color, light.intensity);
    }
    // 每次上传数量（包括0），避免切换场景后沿用上一个场景的灯光。
    shader.setInt("pointLightCount", static_cast<int>(lighting_.pointLights.size()));
    shader.setInt("spotLightCount", static_cast<int>(lighting_.spotLights.size()));
    for (std::size_t i = 0; i < lighting_.pointLights.size(); ++i)
    {
        uploadPoint(shader, "pointLights[" + std::to_string(i) + "]", lighting_.pointLights[i]);
    }
    for (std::size_t i = 0; i < lighting_.spotLights.size(); ++i)
    {
        const auto &light = lighting_.spotLights[i];
        const auto prefix = "spotLights[" + std::to_string(i) + "]";
        uploadPoint(shader, prefix, light);
        shader.setVec3(prefix + ".direction", light.direction);
        shader.setFloat(prefix + ".innerCos", std::cos(light.innerAngle));
        shader.setFloat(prefix + ".outerCos", std::cos(light.outerAngle));
    }
}
