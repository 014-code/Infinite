#include "TestSupport.h"
#include "graphics/lighting/Lighting.h"
#include "graphics/lighting/SceneLighting.h"
#include "textured_cube/CubeGeometry.h"
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <iostream>

int main()
{
    try
    {
        DirectionalLight light;
        SceneLighting sceneLighting;
        sceneLighting.ambient() = {0.03f, 0.04f, 0.05f};
        sceneLighting.additionalDirectionalLights.push_back({{0, -1, 0}, {1, 1, 1}, 0.5f});
        sceneLighting.pointLights.push_back({{0, 1, 0}, {1, 0.8f, 0.6f}, 2, 5});
        SpotLight spot;
        spot.position = {0, 2, 0}; spot.direction = {0, -1, 0}; spot.innerAngle = 0.2f; spot.outerAngle = 0.5f;
        sceneLighting.spotLights.push_back(spot);
        sceneLighting.validate();
        require(&sceneLighting.ambient() == &sceneLighting.mainLight().ambient,
            "Legacy ambient and scene ambient must be a single value");
        sceneLighting.additionalDirectionalLights.resize(SceneLighting::maxDirectionalLights);
        expectThrow<std::invalid_argument>([&] { sceneLighting.validate(); }, "Too many directional lights accepted");
        sceneLighting = {};
        sceneLighting.pointLights.resize(SceneLighting::maxPointLights + 1);
        expectThrow<std::invalid_argument>([&] { sceneLighting.validate(); }, "Too many point lights accepted");
        sceneLighting = {};
        spot.innerAngle = spot.outerAngle;
        sceneLighting.spotLights.push_back(spot);
        expectThrow<std::invalid_argument>([&] { sceneLighting.validate(); }, "Invalid spot angles accepted");

        for (float magnitude : {1e-35f, 1e35f})
        {
            light.direction = {magnitude, 0, 0};
            require(normalizedLightDirection(light) == glm::vec3(1, 0, 0), "Unstable light normalization");
        }
        light.direction = glm::vec3(0);
        expectThrow<std::invalid_argument>([&] { normalizedLightDirection(light); }, "Zero direction accepted");
        light = {};
        light.direction.x = std::numeric_limits<float>::quiet_NaN();
        expectThrow<std::invalid_argument>([&] { validateDirectionalLight(light); }, "NaN direction accepted");
        light = {};
        light.ambient.x = -1;
        expectThrow<std::invalid_argument>([&] { validateDirectionalLight(light); }, "Negative ambient accepted");
        light = {};
        light.intensity = std::numeric_limits<float>::infinity();
        expectThrow<std::invalid_argument>([&] { validateDirectionalLight(light); }, "Infinite intensity accepted");

        // 验证逆转置的几何定义：变换后的法线仍垂直于变换后的表面切向量。
        auto world = glm::rotate(glm::mat4(1), 0.7f, glm::vec3(0, 1, 0));
        world = glm::scale(world, glm::vec3(-2, 1, 0.5f));
        const auto normal = lightingNormalMatrix(world) * glm::vec3(1, 0, 1);
        const auto tangent = glm::mat3(world) * glm::vec3(1, 0, -1);
        require(std::abs(glm::dot(normal, tangent)) < 0.0001f, "Normal is no longer perpendicular");
        expectThrow<std::invalid_argument>([]
        { lightingNormalMatrix(glm::scale(glm::mat4(1), glm::vec3(0, 1, 1))); }, "Zero scale accepted");

        // 每个立方体面的法线必须朝外；默认+Z会让其余五个面产生错误光照。
        const auto cube = CubeExample::vertices();
        for (const auto &vertex : cube)
        {
            require(std::abs(glm::length(vertex.normal) - 1) < 0.0001f &&
                std::abs(glm::dot(vertex.position, vertex.normal) - 0.5f) < 0.0001f,
                "Cube normal is not outward and unit length");
        }
        std::cout << "Lighting CPU validation passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
