#include "MenuStates.h"

#include "game/state/ApplicationStateStack.h"

#include <utility>

MainMenuState::MainMenuState(MainMenuActions actions)
    : MenuState("MAIN MENU", "SELECT AN OPTION",
        {
            {"START", actions.start},
            {"QUIT", actions.quit}
        },
        {false, false, false, true},
        [](ApplicationStateStack &states) { states.quit(); })
{
}

PauseState::PauseState(PauseActions actions)
    : MenuState("PAUSED", "THE GAME IS PAUSED",
        {
            {"RESUME", actions.resume},
            {"RESTART", actions.restart},
            {"MAIN MENU", actions.mainMenu}
        },
        {false, false, true, true},
        [resume = actions.resume](ApplicationStateStack &states)
        {
            if (resume) { resume(states); }
        })
{
}

ResultState::ResultState(std::string resultText, ResultActions actions)
    : MenuState(std::move(resultText), "CHOOSE WHAT TO DO NEXT",
        {
            {"RESTART", actions.restart},
            {"MAIN MENU", actions.mainMenu}
        },
        {false, false, true, true},
        [mainMenu = actions.mainMenu](ApplicationStateStack &states)
        {
            if (mainMenu) { mainMenu(states); }
        })
{
}
