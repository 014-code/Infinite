#pragma once

#include "MenuState.h"

// 这些具体类型只提供常用菜单的标准按钮布局；按钮动作仍由应用传入。
struct MainMenuActions
{
    std::function<void(ApplicationStateStack &)> start;
    std::function<void(ApplicationStateStack &)> quit;
};

class MainMenuState final : public MenuState
{
public:
    explicit MainMenuState(MainMenuActions actions);
};

struct PauseActions
{
    std::function<void(ApplicationStateStack &)> resume;
    std::function<void(ApplicationStateStack &)> restart;
    std::function<void(ApplicationStateStack &)> mainMenu;
};

class PauseState final : public MenuState
{
public:
    explicit PauseState(PauseActions actions);
};

struct ResultActions
{
    std::function<void(ApplicationStateStack &)> restart;
    std::function<void(ApplicationStateStack &)> mainMenu;
};

class ResultState final : public MenuState
{
public:
    ResultState(std::string resultText, ResultActions actions);
};
