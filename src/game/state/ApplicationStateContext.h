#pragma once

#include <memory>

class Application;
class ApplicationState;
class ApplicationStateStack;

// 状态回调使用的窄上下文。
// 它让状态可以访问Application服务和请求切换，但不能直接修改状态栈容器。
class ApplicationStateContext final
{
public:
    Application &application() const noexcept;
    ApplicationStateStack &states() const noexcept;

    void push(std::unique_ptr<ApplicationState> state) const;
    void pop() const;
    void replace(std::unique_ptr<ApplicationState> state) const;
    void clear() const;
    void quit() const;

private:
    friend class ApplicationStateStack;
    ApplicationStateContext(Application &application, ApplicationStateStack &states) noexcept
        : application_(&application), states_(&states)
    {
    }

    Application *application_ = nullptr;
    ApplicationStateStack *states_ = nullptr;
};
