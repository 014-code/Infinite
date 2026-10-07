#include "../common/ExampleRun.h"
#include "MiniGameScene.h"

#include <memory>

int main(int argc, char *argv[])
{
    std::shared_ptr<MiniGameExample::GameState> state;
    return runExample(argc, argv, "mini_game",
        "Infinite - Mini Game | collect the energy avocados",
        {0.025f, 0.04f, 0.075f, 1.0f},
        [&state](Application &application, const std::filesystem::path &directory)
        {
            state = MiniGameExample::createScene(application, directory);
        }, ExampleFrameContent::DrawnScene,
        [&state](Application &application, float deltaTime)
        {
            MiniGameExample::update(application, *state, deltaTime);
        },
        [&state](Application &application, float fixedDeltaTime)
        {
            MiniGameExample::fixedUpdate(application, *state, fixedDeltaTime);
        },
        [&state](Application &application, float interpolationAlpha)
        {
            MiniGameExample::afterPhysics(application, *state, interpolationAlpha);
        });
}
