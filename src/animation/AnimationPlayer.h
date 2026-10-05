#pragma once

#include "AnimationSampler.h"
#include "scene/ModelInstantiator.h"
#include <memory>

// 一个播放器对应一个模型实例。共享只读Model，独享时间/姿态/节点ID。
// 不拥有Scene，也不在析构中访问Scene；每次操作显式传入仍存活的Scene。
// 主线程调用，顺序建议：应用输入 -> player.update -> Scene::update -> 渲染。
class AnimationPlayer final
{
public:
    AnimationPlayer(Scene &scene, std::shared_ptr<const Model> model, const ModelInstance &instance);
    // play切换片段并立即应用第0秒；resume从当前位置继续，pause保留当前姿态。
    void play(Scene &scene, std::size_t clipIndex);
    // 从当前片段时间淡入目标片段；duration=0等价于立即切换。
    void crossFade(Scene &scene, std::size_t clipIndex, float duration);
    void resume() noexcept { playing_ = selected_; }
    void pause() noexcept { playing_ = false; }
    // stop恢复初始姿态并清除当前片段；不会移动用户控制的实例根节点。
    void stop(Scene &scene);
    void seek(Scene &scene, double seconds);
    void update(Scene &scene, float deltaTime);
    void setLooping(bool looping) noexcept { looping_ = looping; }
    void setSpeed(float speed); // 有限、非负，0表示冻结；首版不支持倒放。
    bool isPlaying() const noexcept { return playing_; }
    bool isSelected() const noexcept { return selected_; }
    double time() const noexcept { return time_; }
    float speed() const noexcept { return speed_; }
    // 状态机注册阶段使用，不暴露Model内部容器给调用方。
    void validateClipIndex(std::size_t clipIndex) const;
private:
    void apply(Scene &scene, const std::vector<LocalPose> &pose) const;
    std::shared_ptr<const Model> model_;
    const Scene *owner_;
    ObjectId rootId_;
    std::vector<ObjectId> nodeIds_;
    std::vector<LocalPose> restPose_;
    std::size_t clip_ = 0;
    double time_ = 0;
    float speed_ = 1;
    bool selected_ = false;
    bool playing_ = false;
    bool looping_ = true;
    struct Transition
    {
        bool active = false;
        std::size_t fromClip = 0;
        double fromTime = 0;
        std::size_t toClip = 0;
        double toTime = 0;
        double duration = 0;
        double elapsed = 0;
    } transition_;
};
