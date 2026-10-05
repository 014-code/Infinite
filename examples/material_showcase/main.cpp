#include "../common/ExampleRun.h"
#include "ShowcaseScene.h"

int main(int argc, char *argv[])
{
    FreeCameraController controller(MaterialShowcase::cameraSettings());
    MaterialShowcase::Exhibits exhibits;
    return runExample(argc, argv, "material_showcase",
        "Infinite - Material Gallery | WASD move | RMB look | P rotate | R reset",
        {0.12f, 0.14f, 0.16f, 1},
        [&](Application &application, const std::filesystem::path &directory)
        {
            exhibits = MaterialShowcase::createScene(application, directory);
        }, ExampleFrameContent::DrawnScene,
        [&](Application &application, float deltaTime)
        {
            controller.update(application.camera(), application.input(), deltaTime, application.window().isFocused());
            // 只响应按下边沿；长按P不会在每一帧反复切换，失焦时不处理控制输入。
            if (application.window().isFocused() && application.input().wasKeyPressed(Key::P))
            {
                exhibits.rotating = !exhibits.rotating;
            }
            MaterialShowcase::updateExhibits(application, exhibits, deltaTime);
        });
}
