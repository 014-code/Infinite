#include "common/ExampleRun.h"
#include "common/FreeCameraController.h"
#include "scene/ModelInstantiator.h"
#include "graphics/resources/Material.h"

int main(int argc, char *argv[])
{
    try
    {
        ExampleRun example(argc, argv);
        const auto directory = executableDirectory(argv[0]);
        example.beginLogging(directory, "gltf_pbr");
        ApplicationConfig config;
        config.title = "Infinite - glTF PBR | Original Avocado | WASD / RMB";
        config.visible = example.visible(); config.linearHdr = true;
        Application app(config);
        FreeCameraSettings settings;
        settings.startPosition = {0, 1.8f, 5}; settings.startYaw = glm::radians(-90.0f);
        settings.startPitch = glm::radians(-8.0f);
        FreeCameraController camera(settings);
        auto callbacks = example.callbacks();
        callbacks.initialize = [&](Application &application)
        {
            auto model = application.resources().loadPbrModel(directory / "assets/Avocado.glb");
            auto instance = ModelInstantiator::instantiate(application.scene(), *model, "original avocado");
            auto *root = application.scene().findObject(instance.rootId);
            root->transform.scale = glm::vec3(18);
            root->transform.setEulerAngles({0, .3f, 0});
            PlaneOptions floor;
            floor.size = {8, 8}; floor.material = application.pbrResources().createMaterial({.12f, .13f, .15f, 1});
            application.scene().createPlane(floor).transform.position.y = -.05f;
            application.scene().lighting().mainLight().direction = {-.3f, -.6f, -1};
            application.scene().lighting().mainLight().intensity = 3;
            application.scene().lighting().ambient() = {.08f, .08f, .08f};
            camera.apply(application.camera());
        };
        callbacks.update = [&](Application &application, float dt)
        { camera.update(application.camera(), application.input(), dt, application.window().isFocused()); };
        app.run(callbacks);
        example.finishLogging("gltf_pbr");
        return 0;
    }
    catch (const std::exception &error) { LOG_ERROR(error.what()); return 1; }
}
