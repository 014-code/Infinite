#pragma once

#include "game/state/ApplicationState.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

class Application;
class ApplicationStateStack;
class UiCanvas;

// 菜单按钮只描述显示文本和动作，动作由应用层决定，不把“开始游戏/重开关卡”等规则写进引擎。
struct MenuButtonDefinition
{
    std::string label;
    std::function<void(ApplicationStateStack &)> action;
};

// MenuState负责把一个菜单定义映射到UiCanvas，并处理Esc和UI生命周期。
// 它可以作为MainMenu、Pause和Result状态的共同基类。
class MenuState : public ApplicationState
{
public:
    MenuState(std::string title, std::string subtitle,
        std::vector<MenuButtonDefinition> buttons, StateFramePolicy framePolicy,
        std::function<void(ApplicationStateStack &)> escapeAction = {});

    void onEnter(ApplicationStateContext &context) override;
    void onExit(ApplicationStateContext &context) override;
    void onPause(ApplicationStateContext &context) override;
    void onResume(ApplicationStateContext &context) override;
    void onEvents(ApplicationStateContext &context) override;
    void afterRender(ApplicationStateContext &context) override;

    // 示例或上层应用可用它做菜单动画/冒烟退出；普通菜单不需要设置。
    void setAfterRender(std::function<void(Application &)> callback)
    {
        afterRender_ = std::move(callback);
    }

    StateFramePolicy framePolicy() const noexcept override { return framePolicy_; }

protected:
    // 子类可在UI创建后补充自己的标签或状态信息；返回时Canvas已包含标题和按钮。
    virtual void afterBuild(Application &, UiCanvas &) {}

private:
    void build(Application &application, ApplicationStateStack &states);
    void clear(Application &application) noexcept;

    std::string title_;
    std::string subtitle_;
    std::vector<MenuButtonDefinition> buttons_;
    StateFramePolicy framePolicy_;
    std::function<void(ApplicationStateStack &)> escapeAction_;
    std::function<void(Application &)> afterRender_;
    bool built_ = false;
};
