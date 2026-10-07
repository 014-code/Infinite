#pragma once

#include "ApplicationStateContext.h"
#include "StateFramePolicy.h"

// ApplicationState表示应用流程状态，而不是一个Scene中的GameObject或玩法数据。
// 例如主菜单、游戏中、暂停和结算都可以实现为独立状态。
class ApplicationState
{
public:
    virtual ~ApplicationState() = default;

    virtual void onEnter(ApplicationStateContext &) {}
    virtual void onExit(ApplicationStateContext &) {}
    virtual void onPause(ApplicationStateContext &) {}
    virtual void onResume(ApplicationStateContext &) {}

    virtual void onEvents(ApplicationStateContext &) {}
    virtual void update(ApplicationStateContext &, float) {}
    virtual void fixedUpdate(ApplicationStateContext &, float) {}
    virtual void afterPhysics(ApplicationStateContext &, float) {}
    virtual void afterRender(ApplicationStateContext &) {}

    // 返回当前状态对共享Application主循环的控制策略。
    virtual StateFramePolicy framePolicy() const noexcept { return {}; }
};
