#include "MenuState.h"

#include "game/state/ApplicationStateStack.h"
#include "ui/UiButton.h"
#include "ui/UiCanvas.h"
#include "ui/UiLabel.h"
#include "ui/UiPanel.h"

#include "core/Application.h"

#include <algorithm>
#include <utility>

MenuState::MenuState(std::string title, std::string subtitle,
    std::vector<MenuButtonDefinition> buttons, StateFramePolicy framePolicy,
    std::function<void(ApplicationStateStack &)> escapeAction)
    : title_(std::move(title)), subtitle_(std::move(subtitle)),
      buttons_(std::move(buttons)), framePolicy_(framePolicy),
      escapeAction_(std::move(escapeAction))
{
}

void MenuState::onEnter(ApplicationStateContext &context)
{
    build(context.application(), context.states());
}

void MenuState::onExit(ApplicationStateContext &context)
{
    clear(context.application());
}

void MenuState::onPause(ApplicationStateContext &context)
{
    // 上层状态要暂时接管同一个Canvas；恢复时再按原定义重建。
    clear(context.application());
}

void MenuState::onResume(ApplicationStateContext &context)
{
    build(context.application(), context.states());
}

void MenuState::onEvents(ApplicationStateContext &context)
{
    if (context.application().input().wasKeyPressed(Key::Escape) && escapeAction_)
    {
        escapeAction_(context.states());
    }
}

void MenuState::afterRender(ApplicationStateContext &context)
{
    if (afterRender_)
    {
        afterRender_(context.application());
    }
}

void MenuState::clear(Application &application) noexcept
{
    application.ui().clear();
    built_ = false;
}

void MenuState::build(Application &application, ApplicationStateStack &states)
{
    auto &canvas = application.ui();
    canvas.clear();

    const glm::ivec2 windowSize = application.window().size();
    const float width = std::max(640.0f, static_cast<float>(windowSize.x));
    const float height = std::max(480.0f, static_cast<float>(windowSize.y));
    const float panelWidth = std::min(560.0f, width - 48.0f);
    const float buttonHeight = 58.0f;
    const float buttonSpacing = 14.0f;
    const float panelHeight = 148.0f +
        static_cast<float>(buttons_.size()) * buttonHeight +
        static_cast<float>(buttons_.empty() ? 0 : buttons_.size() - 1) * buttonSpacing;

    auto &panel = canvas.create<UiPanel>(glm::vec4(0.07f, 0.09f, 0.15f, 0.97f));
    panel.setRect({(width - panelWidth) * 0.5f, (height - panelHeight) * 0.5f,
        panelWidth, panelHeight});

    auto &title = panel.createChild<UiLabel>(title_);
    title.setRect({24.0f, 22.0f, panelWidth - 48.0f, 42.0f});
    title.setScale(3.0f);
    title.setAlignment(UiTextAlign::Center);

    if (!subtitle_.empty())
    {
        auto &subtitle = panel.createChild<UiLabel>(subtitle_);
        subtitle.setRect({24.0f, 68.0f, panelWidth - 48.0f, 26.0f});
        subtitle.setScale(1.5f);
        subtitle.setColor({0.64f, 0.72f, 0.86f, 1.0f});
        subtitle.setAlignment(UiTextAlign::Center);
    }

    float buttonY = 112.0f;
    for (const auto &definition : buttons_)
    {
        auto &button = panel.createChild<UiButton>(definition.label);
        button.setRect({56.0f, buttonY, panelWidth - 112.0f, buttonHeight});
        const auto action = definition.action;
        button.setOnActivated([action, &states]
        {
            if (action)
            {
                // 状态切换仍然通过ApplicationStateStack排队，不在按钮树中直接改状态容器。
                action(states);
            }
        });
        buttonY += buttonHeight + buttonSpacing;
    }

    afterBuild(application, canvas);
    built_ = true;
}
