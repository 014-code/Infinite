#include "ApplicationStateStack.h"

#include "core/Application.h"

#include <stdexcept>
#include <utility>

ApplicationStateStack::~ApplicationStateStack()
{
    // Application正常运行结束时会调用stop；如果调用方在run前销毁栈，
    // 此处不能访问不存在的Application，只释放状态对象本身。
    if (started_)
    {
        stop();
    }
}

void ApplicationStateStack::ensureState(const std::unique_ptr<ApplicationState> &state) const
{
    if (!state)
    {
        throw std::invalid_argument("Application state must not be null");
    }
}

void ApplicationStateStack::push(std::unique_ptr<ApplicationState> state)
{
    ensureState(state);
    if (!started_)
    {
        if (!states_.empty())
        {
            throw std::logic_error("Only one initial application state may be pushed");
        }
        states_.push_back(std::move(state));
        return;
    }

    request({StateTransitionType::Push, std::move(state)});
}

void ApplicationStateStack::pop()
{
    if (!started_)
    {
        throw std::logic_error("Cannot pop an application state before run");
    }
    request({StateTransitionType::Pop, nullptr});
}

void ApplicationStateStack::replace(std::unique_ptr<ApplicationState> state)
{
    ensureState(state);
    if (!started_)
    {
        throw std::logic_error("Cannot replace an application state before run");
    }
    request({StateTransitionType::Replace, std::move(state)});
}

void ApplicationStateStack::clear()
{
    if (!started_)
    {
        states_.clear();
        return;
    }
    request({StateTransitionType::Clear, nullptr});
}

void ApplicationStateStack::quit()
{
    if (!started_)
    {
        throw std::logic_error("Cannot quit an application before run");
    }
    request({StateTransitionType::Quit, nullptr});
}

ApplicationState *ApplicationStateStack::current() noexcept
{
    return states_.empty() ? nullptr : states_.back().get();
}

const ApplicationState *ApplicationStateStack::current() const noexcept
{
    return states_.empty() ? nullptr : states_.back().get();
}

StateFramePolicy ApplicationStateStack::framePolicy() const noexcept
{
    const auto *state = current();
    return state ? state->framePolicy() : StateFramePolicy{};
}

void ApplicationStateStack::start(Application &application)
{
    if (started_)
    {
        throw std::logic_error("Application state stack has already started");
    }
    if (states_.empty())
    {
        throw std::logic_error("Application state stack requires an initial state");
    }
    application_ = &application;
    started_ = true;
    ApplicationStateContext stateContext = context();
    current()->onEnter(stateContext);
    commitPending();
}

void ApplicationStateStack::stop() noexcept
{
    // 析构路径不能让用户状态异常传播；Application正常run时，
    // 状态回调异常会在更早的主循环中被捕获并保留原始异常。
    try
    {
        if (started_ && application_ && !states_.empty())
        {
            dispatching_ = true;
            ApplicationStateContext stateContext = context();
            while (!states_.empty())
            {
                states_.back()->onExit(stateContext);
                states_.pop_back();
            }
            dispatching_ = false;
        }
    }
    catch (...)
    {
        dispatching_ = false;
        states_.clear();
    }
    pending_.clear();
    application_ = nullptr;
    started_ = false;
}

void ApplicationStateStack::request(StateTransition transition)
{
    if (!started_)
    {
        throw std::logic_error("Application state transition requested before run");
    }
    pending_.push_back(std::move(transition));
}

ApplicationStateContext ApplicationStateStack::context()
{
    return ApplicationStateContext(*application_, *this);
}

void ApplicationStateStack::commitPending()
{
    if (!started_ || dispatching_)
    {
        throw std::logic_error("Cannot commit application states during a state callback");
    }

    // 状态回调可以在onEnter/onExit中继续排队命令；限制单次提交数量，
    // 防止错误的状态逻辑形成无限Push/Replace循环而卡死主线程。
    constexpr std::size_t kMaximumTransitionsPerCommit = 1024;
    std::size_t committed = 0;
    while (!pending_.empty())
    {
        if (++committed > kMaximumTransitionsPerCommit)
        {
            pending_.clear();
            throw std::logic_error("Too many application state transitions in one frame");
        }

        StateTransition transition = std::move(pending_.front());
        pending_.erase(pending_.begin());
        ApplicationStateContext stateContext = context();
        switch (transition.type)
        {
        case StateTransitionType::Push:
            ensureState(transition.state);
            if (current())
            {
                current()->onPause(stateContext);
            }
            states_.push_back(std::move(transition.state));
            current()->onEnter(stateContext);
            break;
        case StateTransitionType::Pop:
            if (!states_.empty())
            {
                current()->onExit(stateContext);
                states_.pop_back();
            }
            if (current())
            {
                current()->onResume(stateContext);
            }
            else
            {
                application_->requestClose();
            }
            break;
        case StateTransitionType::Replace:
            ensureState(transition.state);
            if (!states_.empty())
            {
                current()->onExit(stateContext);
                states_.pop_back();
            }
            states_.push_back(std::move(transition.state));
            current()->onEnter(stateContext);
            break;
        case StateTransitionType::Clear:
            while (!states_.empty())
            {
                current()->onExit(stateContext);
                states_.pop_back();
            }
            application_->requestClose();
            break;
        case StateTransitionType::Quit:
            application_->requestClose();
            break;
        }
    }
}

void ApplicationStateStack::dispatchEvents()
{
    if (!started_ || !current())
    {
        return;
    }
    dispatching_ = true;
    ApplicationStateContext stateContext = context();
    try { current()->onEvents(stateContext); }
    catch (...) { dispatching_ = false; throw; }
    dispatching_ = false;
}

void ApplicationStateStack::dispatchUpdate(float deltaTime)
{
    if (!started_ || !current())
    {
        return;
    }
    dispatching_ = true;
    ApplicationStateContext stateContext = context();
    try { current()->update(stateContext, deltaTime); }
    catch (...) { dispatching_ = false; throw; }
    dispatching_ = false;
}

void ApplicationStateStack::dispatchFixedUpdate(float fixedDeltaTime)
{
    if (!started_ || !current())
    {
        return;
    }
    dispatching_ = true;
    ApplicationStateContext stateContext = context();
    try { current()->fixedUpdate(stateContext, fixedDeltaTime); }
    catch (...) { dispatching_ = false; throw; }
    dispatching_ = false;
}

void ApplicationStateStack::dispatchAfterPhysics(float interpolationAlpha)
{
    if (!started_ || !current())
    {
        return;
    }
    dispatching_ = true;
    ApplicationStateContext stateContext = context();
    try { current()->afterPhysics(stateContext, interpolationAlpha); }
    catch (...) { dispatching_ = false; throw; }
    dispatching_ = false;
}

void ApplicationStateStack::dispatchAfterRender()
{
    if (!started_ || !current())
    {
        return;
    }
    dispatching_ = true;
    ApplicationStateContext stateContext = context();
    try { current()->afterRender(stateContext); }
    catch (...) { dispatching_ = false; throw; }
    dispatching_ = false;
}

Application &ApplicationStateContext::application() const noexcept
{
    return *application_;
}

ApplicationStateStack &ApplicationStateContext::states() const noexcept
{
    return *states_;
}

void ApplicationStateContext::push(std::unique_ptr<ApplicationState> state) const
{
    states_->push(std::move(state));
}

void ApplicationStateContext::pop() const
{
    states_->pop();
}

void ApplicationStateContext::replace(std::unique_ptr<ApplicationState> state) const
{
    states_->replace(std::move(state));
}

void ApplicationStateContext::clear() const
{
    states_->clear();
}

void ApplicationStateContext::quit() const
{
    states_->quit();
}
