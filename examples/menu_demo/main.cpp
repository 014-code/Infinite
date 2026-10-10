#include "../common/ExampleRun.h"

#include "game/menu/MenuStates.h"
#include "game/state/ApplicationStateStack.h"
#include "ui/UiCanvas.h"
#include "ui/UiLabel.h"

#include <iostream>
#include <memory>
#include <string_view>

namespace
{
    std::unique_ptr<ApplicationState> makeMainMenu(bool smokeTest);
    std::unique_ptr<ApplicationState> makeGameplay();
    std::unique_ptr<ApplicationState> makeResult();

    class GameplayState final : public ApplicationState
    {
    public:
        void onEnter(ApplicationStateContext &context) override { buildUi(context.application()); }

        void onExit(ApplicationStateContext &context) override
        {
            context.application().ui().clear();
        }

        void onPause(ApplicationStateContext &context) override
        {
            context.application().ui().clear();
        }

        void onResume(ApplicationStateContext &context) override
        {
            buildUi(context.application());
        }

        void onEvents(ApplicationStateContext &context) override
        {
            if (context.application().input().wasKeyPressed(Key::Escape))
            {
                context.push(std::make_unique<PauseState>(PauseActions{
                    [](ApplicationStateStack &states) { states.pop(); },
                    [](ApplicationStateStack &states) { states.replace(makeGameplay()); },
                    [](ApplicationStateStack &states) { states.replace(makeMainMenu(false)); }
                }));
            }
            if (context.application().input().wasKeyPressed(Key::R))
            {
                context.replace(makeResult());
            }
        }

    private:
        static void buildUi(Application &application)
        {
            auto &label = application.ui().create<UiLabel>("GAMEPLAY - ESC PAUSE - R RESULT");
            label.setRect({24.0f, 24.0f, 720.0f, 36.0f});
            label.setScale(2.0f);
            label.setColor({0.84f, 0.90f, 1.0f, 1.0f});
        }
    };

    std::unique_ptr<ApplicationState> makeGameplay()
    {
        return std::make_unique<GameplayState>();
    }

    std::unique_ptr<ApplicationState> makeMainMenu(bool smokeTest)
    {
        auto state = std::make_unique<MainMenuState>(MainMenuActions{
            [](ApplicationStateStack &states) { states.replace(makeGameplay()); },
            [](ApplicationStateStack &states) { states.quit(); }
        });
        if (smokeTest)
        {
            auto frames = std::make_shared<int>(0);
            state->setAfterRender([frames](Application &application)
            {
                if (++*frames >= 3)
                {
                    application.requestClose();
                }
            });
        }
        return state;
    }

    std::unique_ptr<ApplicationState> makeResult()
    {
        return std::make_unique<ResultState>("RESULT", ResultActions{
            [](ApplicationStateStack &states) { states.replace(makeGameplay()); },
            [](ApplicationStateStack &states) { states.replace(makeMainMenu(false)); }
        });
    }
}

int main(int argc, char *argv[])
{
    try
    {
        const bool smokeTest = argc == 2 && std::string_view(argv[1]) == "--smoke-test";
        ExampleRun run(argc, argv);
        const auto directory = executableDirectory(argv[0]);
        run.beginLogging(directory, "menu_demo");
        ApplicationConfig config;
        config.title = "Infinite - Menu States";
        config.visible = run.visible();
        config.clearColor = {0.035f, 0.045f, 0.07f, 1.0f};
        Application application(config);
        ApplicationStateStack states;
        states.push(makeMainMenu(smokeTest));
        application.run(states);
        run.finishLogging("menu_demo");
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
