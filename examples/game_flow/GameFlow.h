#pragma once

#include <filesystem>
#include <functional>
#include <memory>

class Application;
class ApplicationState;
class ApplicationStateStack;

namespace GameFlowExample
{
    // GameFlowSession保存示例层的流程数据；引擎只提供状态栈、场景和存档服务，
    // 不知道“到达目标就胜利”这样的具体游戏规则。
    struct GameFlowSession;

    // 由入口创建一次，并在主菜单、Gameplay和结果状态之间共享。
    std::shared_ptr<GameFlowSession> createSession(
        const std::filesystem::path &saveDirectory, bool smokeTest,
        std::function<void()> verifyFrame = {});

    // 注册一个可重复加载的游戏场景。SceneManager只负责调用Loader，
    // 玩家碰撞体和Area事件的绑定仍由Gameplay状态完成。
    void registerScenes(Application &application,
        const std::shared_ptr<GameFlowSession> &session);

    // 创建示例的初始主菜单状态。状态栈引用只在Application::run期间使用。
    std::unique_ptr<ApplicationState> createInitialState(ApplicationStateStack &states,
        const std::shared_ptr<GameFlowSession> &session);
}
