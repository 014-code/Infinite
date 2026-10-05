#pragma once

#include "AnimationPlayer.h"
#include <string>
#include <unordered_map>

// 应用层状态名到Model动画片段索引的轻量映射。它不解析输入、不包含游戏规则。
// 状态切换只负责调用播放器的交叉淡入；攻击、移动等业务条件仍由应用决定。
class AnimationStateMachine final
{
public:
    AnimationStateMachine(Scene &scene, std::shared_ptr<const Model> model,
        const ModelInstance &instance);
    void addState(const std::string &name, std::size_t clipIndex);
    void setInitialState(Scene &scene, const std::string &name);
    void transitionTo(Scene &scene, const std::string &name, float duration);
    void update(Scene &scene, float deltaTime);
    const std::string &state() const noexcept { return state_; }
    AnimationPlayer &player() noexcept { return player_; }
    const AnimationPlayer &player() const noexcept { return player_; }
private:
    AnimationPlayer player_;
    std::unordered_map<std::string, std::size_t> states_;
    std::string state_;
};
