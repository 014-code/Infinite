#include "support/ColorDepthTarget.h"
#include "TestSupport.h"

#include "graphics/lighting/Lighting.h"
#include "graphics/camera/Camera.h"
#include "math/Transform.h"
#include "scene/Scene.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Shader.h"
#include "platform/Window.h"

#include <GL/glew.h>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>

namespace
{
    void expectRed(float expected, const char *label)
    {
        std::array<unsigned char, 4> pixel{};
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        require(std::abs(static_cast<float>(pixel[0]) / 255.0f - expected) < 0.035f,
            std::string(label) + " red channel mismatch");
        require(pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255,
            std::string(label) + " unexpected non-red channels");
    }
}

int main()
{
    try
    {
        expectThrow<std::invalid_argument>([]
        {
            validateDirectionalLight(DirectionalLight{{0.0f, 0.0f, 0.0f}});
        }, "Zero light direction accepted");
        expectThrow<std::invalid_argument>([]
        {
            validateDirectionalLight(DirectionalLight{{0.0f, 0.0f, -1.0f}, glm::vec3(1.0f), -1.0f});
        }, "Negative light intensity accepted");

        Window window(64, 64, "Diffuse Lighting Test", false);
        ColorDepthTarget target;
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_DITHER);
        Shader shader("examples/diffuse_lighting/shaders/diffuse.vert",
            "examples/diffuse_lighting/shaders/diffuse.frag");
        Mesh quad({
            -0.8f, -0.8f, 0.0f, 1, 0, 0, 0, 0,
             0.8f, -0.8f, 0.0f, 1, 0, 0, 1, 0,
             0.8f,  0.8f, 0.0f, 1, 0, 0, 1, 1,
             0.8f,  0.8f, 0.0f, 1, 0, 0, 1, 1,
            -0.8f,  0.8f, 0.0f, 1, 0, 0, 0, 1,
            -0.8f, -0.8f, 0.0f, 1, 0, 0, 0, 0});
        Material material(shader, {1, 1, 1, 1});
        Camera camera;
        camera.setOrthographic(2.0f, -10.0f, 10.0f);
        Renderer renderer;
        Transform transform;
        transform.scale = {2.0f, 1.0f, 0.5f};
        DirectionalLight light;
        light.direction = {0.0f, 0.0f, -1.0f};
        light.color = {1.0f, 0.0f, 0.0f};
        light.intensity = 0.8f;
        light.ambient = {0.1f, 0.0f, 0.0f};
        // 通过Scene覆盖正式调用链；借用的资源比scene先创建，析构顺序安全。
        Scene scene;
        auto &object = scene.createObject("lit quad");
        object.setRenderable(quad, material);
        object.transform = transform;
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ZERO);
        const auto render = [&]
        {
            renderer.clear(0, 0, 0, 1);
            scene.render(renderer, camera, 1.0f, light);
            GLboolean writeDepth = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &writeDepth);
            GLint program = -1;
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            require(!glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && !writeDepth && program == 0,
                "Lit Scene leaked caller state");
        };
        render();
        expectRed(0.9f, "Front-lit nonuniform-scaled quad");

        light.direction = {0.0f, 0.0f, 1.0f};
        render();
        expectRed(0.1f, "Back-lit quad ambient term");

        // 绕Y轴转60度，法线和光源夹角余弦应为0.5。
        light.direction = {0, 0, -1};
        object.transform.setEulerAngles({0, glm::radians(60.0f), 0});
        render();
        expectRed(0.5f, "Rotation changes diffuse term");
        object.transform.setEulerAngles({0, 0, 0});
        camera.setView({0.4f, 0, 3}, {0, 0, 0});
        render();
        expectRed(0.9f, "Camera movement must not rotate the light");
        camera.setView({0, 0, 3}, {0, 0, 0});

        // 使用斜法线才能识别mat3(model)误用：轴向法线即使错变换也可能通过。
        std::vector<Vertex> oblique(3);
        oblique[0].position = {-0.8f, -0.8f, 0};
        oblique[1].position = {0.8f, -0.8f, 0};
        oblique[2].position = {0, 0.8f, 0};
        for (auto &vertex : oblique)
        {
            vertex.normal = glm::normalize(glm::vec3(1, 0, 1));
            vertex.color = {1, 0, 0};
        }
        auto obliqueMesh = std::make_shared<Mesh>(oblique);
        // 保持借用材质有效，测试结束前解除绑定。
        object.setRenderable(*obliqueMesh, material);
        Transform parent;
        parent.scale = {2, 1, 0.5f};
        object.transform.scale = glm::vec3(1);
        object.transform.setParent(&parent);
        render();
        expectRed(0.1f + 0.8f * 4.0f / std::sqrt(17.0f), "Parent nonuniform scale normal matrix");
        parent.scale.x = 0;
        expectThrow<std::invalid_argument>([&] { scene.render(renderer, camera, 1.0f, light); },
            "Singular lit transform accepted");
        scene.clear();
        require(glGetError() == GL_NO_ERROR, "Diffuse lighting left OpenGL errors");
        std::cout << "Diffuse lighting validation and pixels passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
