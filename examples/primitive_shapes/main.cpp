#include "../common/ExampleRun.h"
#include "PrimitiveScene.h"

int main(int argc, char *argv[])
{
    FreeCameraController cameraController(PrimitiveExample::cameraSettings());
    return runExample(argc, argv, "primitive_shapes",
        "Infinite - Built-in Primitive Shapes | WASD / RMB / R / Esc",
        {0.035f, 0.045f, 0.07f, 1.0f},
        [](Application &application, const std::filesystem::path &)
        {
            PrimitiveExample::createScene(application);
        }, ExampleFrameContent::DrawnScene,
        [&cameraController](Application &application, float deltaTime)
        {
            cameraController.update(application.camera(), application.input(), deltaTime,
                application.window().isFocused());
        });
}
