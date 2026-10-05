#include "TestSupport.h"
#include "support/ColorDepthTarget.h"
#include "platform/Window.h"
#include "resources/PrimitiveResources.h"
#include "scene/Scene.h"
#include "graphics/camera/Camera.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Material.h"
#include "graphics/resources/Shader.h"

#include <GL/glew.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    std::array<int, 3> pixel()
    {
        std::array<unsigned char, 4> bytes{};
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
        return {bytes[0], bytes[1], bytes[2]};
    }

    void expectPixel(std::array<int, 3> expected)
    {
        const auto actual = pixel();
        for (int i = 0; i < 3; ++i)
        {
            require(std::abs(actual[i] - expected[i]) <= 4,
                "Scene light pixel " + std::to_string(actual[i]) + " != " + std::to_string(expected[i]));
        }
    }
}

int main()
{
    try
    {
        Window window(64, 64, "Scene light pixels", false);
        ColorDepthTarget target;
        glDisable(GL_DITHER);
        glDisable(GL_FRAMEBUFFER_SRGB);
        PrimitiveResources resources;
        Scene scene(resources);
        PlaneOptions options;
        options.size = {4, 4}; options.color = glm::vec4(1);
        auto &plane = scene.createPlane(options);
        Camera camera;
        camera.setOrthographic(1.5f, .1f, 100);
        camera.setView({0, 3, 0}, {0, 0, 0}, {0, 0, -1});
        Renderer renderer;
        auto &lighting = scene.lighting();
        lighting.ambient() = glm::vec3(0);
        lighting.mainLight().intensity = 0;
        const auto render = [&]
        {
            renderer.clear(0, 0, 0, 1);
            scene.render(renderer, camera, 1);
        };

        // 环境项属于Scene，关主灯后也保留。默认Scene渲染必须使用自己的配置。
        lighting.ambient() = {.2f, .1f, .05f};
        render(); expectPixel({51, 26, 13});
        lighting.ambient() = glm::vec3(0);
        lighting.mainLight().direction = {0, -1, 0};
        lighting.mainLight().color = {1, 0, 0};
        lighting.mainLight().intensity = .25f;
        lighting.additionalDirectionalLights.push_back({{0, -1, 0}, {0, 1, 0}, .5f});
        render(); expectPixel({64, 128, 0});
        lighting.mainLight().intensity = 0;
        lighting.additionalDirectionalLights.clear();

        // 距离2、range4：((1-(2/4)^4)^2)/4 = 0.2197，即RGBA8中的56。
        lighting.pointLights.push_back({{0, 2, 0}, {1, 1, 1}, 1, 4});
        render(); expectPixel({56, 56, 56});
        lighting.pointLights[0].position.y = 1;
        render(); expectPixel({253, 253, 253});
        lighting.pointLights[0].position.y = 2;
        lighting.pointLights[0].range = 1;
        render(); expectPixel({0, 0, 0});

        // 平移物体和摄像机、保持局部像素观察位置不变，必须让灯相对表面变远。
        lighting.pointLights[0].range = 4;
        plane.transform.position = {10, 0, 0};
        camera.setView({10, 3, 0}, {10, 0, 0}, {0, 0, -1});
        render(); expectPixel({0, 0, 0});
        plane.transform.position = {0, 0, 0};
        camera.setView({0, 3, 0}, {0, 0, 0}, {0, 0, -1});
        lighting.pointLights.clear();

        SpotLight spot;
        spot.position = {0, 2, 0}; spot.range = 4;
        spot.innerAngle = .2f; spot.outerAngle = .6f;
        lighting.spotLights.push_back(spot);
        render(); expectPixel({56, 56, 56});
        lighting.spotLights[0].direction = {std::sin(.45f), -std::cos(.45f), 0};
        render();
        require(pixel()[0] > 10 && pixel()[0] < 50, "Spot penumbra is not gradual");
        lighting.spotLights[0].direction = {1, 0, 0};
        render(); expectPixel({0, 0, 0});
        lighting.spotLights.clear();

        PointLight point{{0, 2, 0}, {1, 0, 0}, .25f, 4};
        lighting.pointLights.assign(SceneLighting::maxPointLights, point);
        spot.color = {0, 1, 0}; spot.intensity = .25f;
        lighting.spotLights.assign(SceneLighting::maxSpotLights, spot);
        render(); expectPixel({112, 56, 0});

        // 清除光源后数量uniform也要变为0，不能沿用前一帧的数组。
        lighting.pointLights.clear(); lighting.spotLights.clear();
        render(); expectPixel({0, 0, 0});
        lighting.pointLights.push_back(point);
        Shader legacy("examples/diffuse_lighting/shaders/diffuse.vert",
            "examples/diffuse_lighting/shaders/diffuse.frag");
        Material legacyMaterial(legacy);
        expectThrow<std::invalid_argument>([&]
        {
            renderer.draw(*plane.mesh(), legacyMaterial, plane.transform, camera, 1, lighting);
        }, "Legacy single-light shader accepted multiple lights");

        lighting.pointLights[0].range = std::numeric_limits<float>::quiet_NaN();
        glDisable(GL_DEPTH_TEST);
        glUseProgram(0);
        expectThrow<std::invalid_argument>([&] { scene.render(renderer, camera, 1); }, "Invalid light accepted");
        GLint program = -1;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        require(program == 0 && !glIsEnabled(GL_DEPTH_TEST), "Invalid light changed OpenGL state");
        require(glGetError() == GL_NO_ERROR, "Scene lighting left GL errors");
        std::cout << "Scene ambient, directional/point/spot pixels, limits and compatibility passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
