#include "../common/ExampleRun.h"
#include "SceneLightingScene.h"

int main(int argc, char *argv[])
{
    FreeCameraController cameraController(SceneLightingExample::cameraSettings());
    return runExample(argc, argv, "scene_lighting",
        "Infinite - Scene Lighting | WASD / RMB / R / Esc",
        {0.02f, 0.025f, 0.04f, 1.0f},
        [](Application &application, const std::filesystem::path &)
        {
            SceneLightingExample::createScene(application);
        }, ExampleFrameContent::DrawnScene,
        [&cameraController](Application &application, float deltaTime)
        {
            cameraController.update(application.camera(), application.input(), deltaTime,
                application.window().isFocused());
        });
}
