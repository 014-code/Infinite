#pragma once

#include "ApplicationState.h"
#include "StateTransition.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

class Application;

// ApplicationStateStack拥有状态对象，并在安全边界按顺序执行排队的切换命令。
//
// 调用约定：
// - run前调用push建立初始状态。
// - 状态回调内调用push/pop/replace/clear/quit只会排队，不会立即销毁当前状态。
// - Application每帧事件阶段开始前提交上一帧的命令。
class ApplicationStateStack final
{
public:
    ApplicationStateStack() = default;
    ~ApplicationStateStack();

    ApplicationStateStack(const ApplicationStateStack &) = delete;
    ApplicationStateStack &operator=(const ApplicationStateStack &) = delete;
    ApplicationStateStack(ApplicationStateStack &&) = delete;
    ApplicationStateStack &operator=(ApplicationStateStack &&) = delete;

    // run前建立初始状态；run开始后同名接口自动变为排队Push，便于状态代码使用。
    void push(std::unique_ptr<ApplicationState> state);

    template<class State, class... Arguments>
    void push(Arguments &&...arguments)
    {
        push(std::make_unique<State>(std::forward<Arguments>(arguments)...));
    }

    void pop();
    void replace(std::unique_ptr<ApplicationState> state);

    template<class State, class... Arguments>
    void replace(Arguments &&...arguments)
    {
        replace(std::make_unique<State>(std::forward<Arguments>(arguments)...));
    }

    void clear();
    void quit();

    bool empty() const noexcept { return states_.empty(); }
    std::size_t size() const noexcept { return states_.size(); }
    ApplicationState *current() noexcept;
    const ApplicationState *current() const noexcept;
    StateFramePolicy framePolicy() const noexcept;

private:
    friend class Application;
    friend class ApplicationStateContext;

    void start(Application &application);
    void stop() noexcept;
    void commitPending();
    void dispatchEvents();
    void dispatchUpdate(float deltaTime);
    void dispatchFixedUpdate(float fixedDeltaTime);
    void dispatchAfterPhysics(float interpolationAlpha);
    void dispatchAfterRender();

    void request(StateTransition transition);
    ApplicationStateContext context();
    void ensureState(const std::unique_ptr<ApplicationState> &state) const;

    Application *application_ = nullptr;
    std::vector<std::unique_ptr<ApplicationState>> states_;
    std::vector<StateTransition> pending_;
    bool started_ = false;
    bool dispatching_ = false;
};
