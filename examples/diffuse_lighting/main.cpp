#include "../common/ExampleRun.h"
#include "../common/FreeCameraController.h"
#include "../textured_cube/CubeGeometry.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"

int main(int argc, char *argv[])
{
    FreeCameraController controller;
    return runExample(argc, argv, "diffuse_lighting",
        "Infinite - Diffuse Lighting (WASD / RMB / R reset / Esc)",
        {0.025f, 0.035f, 0.055f, 1.0f},
        [&controller](Application &application, const std::filesystem::path &directory)
        {
            auto shader = application.resources().loadShader(
                directory / "shaders/diffuse.vert", directory / "shaders/diffuse.frag");
            auto material = std::make_shared<Material>(shader, glm::vec4(0.9f, 0.55f, 0.22f, 1.0f));
            material->setCullMode(CullMode::Back);
            material->setCorrectMirroredWinding(true);
            auto mesh = std::make_shared<Mesh>(CubeExample::vertices());
            auto &cube = application.scene().createObject("diffuse cube");
            cube.setRenderable(mesh, material);
            // 刻意使用非均匀缩放，让示例同时覆盖法线逆转置变换。
            cube.transform.scale = {1.0f, 0.8f, 1.25f};
            cube.transform.setEulerAngles({0.25f, 0.35f, 0.0f});
            cube.script().setUpdateCallback([](GameObject &object, float deltaTime)
            {
                object.transform.rotateEuler({deltaTime * 0.25f, deltaTime * 0.5f, 0.0f});
            });
            auto &light = application.directionalLight();
            light.direction = {-0.5f, -0.8f, -1.0f};
            light.intensity = 0.8f;
            light.ambient = glm::vec3(0.12f);
            controller.apply(application.camera());
        }, ExampleFrameContent::DrawnScene,
        [&controller](Application &application, float deltaTime)
        {
            controller.update(application.camera(), application.input(), deltaTime,
                application.window().isFocused());
        });
}
