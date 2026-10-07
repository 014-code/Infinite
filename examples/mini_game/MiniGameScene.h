#pragma once

#include <filesystem>
#include <memory>

class Application;

namespace MiniGameExample
{
    // 游戏运行状态由示例持有，不进入引擎Scene，避免把“收集物”和“胜利条件”硬编码进框架。
    struct GameState;

    // 创建一份可直接运行的收集小游戏场景，并返回跨帧保存的玩法状态。
    std::shared_ptr<GameState> createScene(Application &application,
        const std::filesystem::path &directory);

    // 普通帧：采样输入边沿、更新计时器和第三人称摄像机。
    void update(Application &application, GameState &state, float deltaTime);

    // 固定物理步：只在这里修改CharacterBody的速度并推进角色碰撞。
    void fixedUpdate(Application &application, GameState &state, float fixedDeltaTime);

    // 物理同步后：检查玩家是否碰到收集物，并让摄像机跟随最新玩家位置。
    void afterPhysics(Application &application, GameState &state, float interpolationAlpha);
}
