#include "AnimationStateMachine.h"

#include <cmath>
#include <stdexcept>

AnimationStateMachine::AnimationStateMachine(Scene &scene, std::shared_ptr<const Model> model,
    const ModelInstance &instance)
    : player_(scene, std::move(model), instance)
{
}

void AnimationStateMachine::addState(const std::string &name, std::size_t clipIndex)
{
    if (name.empty()) { throw std::invalid_argument("Animation state name must not be empty"); }
    // 让播放器用Model自身的at做边界检查，状态表只保存稳定的片段索引。
    player_.validateClipIndex(clipIndex);
    if (!states_.emplace(name, clipIndex).second)
    { throw std::invalid_argument("Animation state already exists: " + name); }
}

void AnimationStateMachine::setInitialState(Scene &scene, const std::string &name)
{
    const auto found = states_.find(name);
    if (found == states_.end()) { throw std::invalid_argument("Unknown animation state: " + name); }
    player_.play(scene, found->second);
    state_ = name;
}

void AnimationStateMachine::transitionTo(Scene &scene, const std::string &name, float duration)
{
    const auto found = states_.find(name);
    if (found == states_.end()) { throw std::invalid_argument("Unknown animation state: " + name); }
    if (!std::isfinite(duration) || duration < 0) { throw std::invalid_argument("Invalid animation transition duration"); }
    if (state_ == name) { return; }
    if (!player_.isSelected()) { player_.play(scene, found->second); }
    else { player_.crossFade(scene, found->second, duration); }
    state_ = name;
}

void AnimationStateMachine::update(Scene &scene, float deltaTime)
{
    player_.update(scene, deltaTime);
}
