#include "../common/ExampleRun.h"
#include "GameFlow.h"

#include "game/state/ApplicationStateStack.h"

#include <iostream>
#include <memory>
#include <string_view>

int main(int argc, char *argv[])
{
    try
    {
        ExampleRun run(argc, argv, ExampleFrameContent::DrawnScene);
        const bool smokeTest = argc == 2 && std::string_view(argv[1]) == "--smoke-test";
        const auto directory = executableDirectory(argv[0]);
        run.beginLogging(directory, "game_flow");

        ApplicationConfig config;
        config.title = "Infinite - Game Flow";
        config.visible = run.visible();
        config.clearColor = {0.025f, 0.04f, 0.075f, 1.0f};
        Application application(config);

        auto session = GameFlowExample::createSession(
            directory / "saves" / "game_flow", smokeTest,
            [&run] { run.verifyFrame(); });
        GameFlowExample::registerScenes(application, session);

        ApplicationStateStack states;
        states.push(GameFlowExample::createInitialState(states, session));
        application.run(states);
        run.finishLogging("game_flow");
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
