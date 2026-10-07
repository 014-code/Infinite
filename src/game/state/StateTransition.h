#pragma once

#include <memory>

class ApplicationState;

enum class StateTransitionType
{
    Push,
    Pop,
    Replace,
    Clear,
    Quit
};

// 状态切换命令只在ApplicationStateStack的安全边界执行。
// unique_ptr保证排队期间新状态有明确所有权，切换失败时也不会泄漏。
struct StateTransition
{
    StateTransitionType type = StateTransitionType::Pop;
    std::unique_ptr<ApplicationState> state;
};
