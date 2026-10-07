#include "AnimationPlayer.h"
#include "scene/Scene.h"
#include "AnimationBlender.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    void emitEvents(const AnimationClip &clip, double from, double to, bool looping,
        const AnimationPlayer::EventCallback &callback)
    {
        if (!callback || clip.events().empty() || to == from)
        {
            return;
        }
        const auto emitRange = [&](double begin, double end)
        {
            for (const auto &event : clip.events())
            {
                if (event.time > begin && event.time <= end)
                {
                    callback(event);
                }
            }
        };
        if (looping && to < from)
        {
            emitRange(from, static_cast<double>(clip.duration()));
            emitRange(-1.0, to);
        }
        else
        {
            emitRange(from, to);
        }
    }
}

AnimationPlayer::AnimationPlayer(Scene &scene, std::shared_ptr<const Model> model, const ModelInstance &instance)
    : model_(std::move(model)), owner_(&scene), rootId_(instance.rootId), nodeIds_(instance.nodeIds)
{
    if (!model_ || instance.owner != &scene || instance.sourceModel != model_.get() ||
        nodeIds_.size() != model_->nodes().size() || rootId_ == 0)
    { throw std::invalid_argument("AnimationPlayer requires a matching model/scene instance"); }
    for (const auto &node : model_->nodes())
    { restPose_.push_back({node.position, node.rotation, node.scale}); }
    // 只检查绑定，不改变已有姿态；play/stop才负责提交姿态。
    if (!scene.findObject(rootId_)) { throw std::invalid_argument("Model instance root was removed"); }
    for (const auto id : nodeIds_)
    { if (!scene.findObject(id)) { throw std::invalid_argument("Model instance node was removed"); } }
}

void AnimationPlayer::apply(Scene &scene, const std::vector<LocalPose> &pose) const
{
    if (&scene != owner_ || !scene.findObject(rootId_))
    { throw std::runtime_error("Animation instance does not exist in this Scene"); }
    std::vector<GameObject *> targets;
    targets.reserve(nodeIds_.size());
    // 先解析全部ID，再写入。中途删过节点时明确失败，不能更新半个模型。
    for (auto id : nodeIds_)
    {
        auto *object = scene.findObject(id);
        if (!object) { throw std::runtime_error("Animation target node was removed"); }
        targets.push_back(object);
    }
    for (std::size_t index = 0; index < targets.size(); ++index)
    {
        auto &transform = targets[index]->transform;
        transform.position = pose[index].position;
        transform.setRotation(pose[index].rotation);
        transform.scale = pose[index].scale;
    }
}

void AnimationPlayer::play(Scene &scene, std::size_t clipIndex)
{
    validateClipIndex(clipIndex);
    const auto &clip = model_->animations().at(clipIndex);
    apply(scene, AnimationSampler::sample(clip, 0, restPose_));
    clip_ = clipIndex; time_ = 0; selected_ = true; playing_ = true; transition_ = {};
}

void AnimationPlayer::crossFade(Scene &scene, std::size_t clipIndex, float duration)
{
    validateClipIndex(clipIndex);
    if (!std::isfinite(duration) || duration < 0) { throw std::invalid_argument("Invalid animation fade duration"); }
    if (!selected_ || duration == 0) { play(scene, clipIndex); return; }
    if (clipIndex == clip_ && !transition_.active) { return; }
    transition_.active = true;
    transition_.fromClip = transition_.active ? clip_ : transition_.toClip;
    transition_.fromTime = time_;
    transition_.toClip = clipIndex;
    transition_.toTime = 0;
    transition_.duration = duration;
    transition_.elapsed = 0;
    // 目标片段从0秒姿态开始，下一次update会推进两边的独立时间。
    apply(scene, AnimationBlender::blend(
        AnimationSampler::sample(model_->animations()[transition_.fromClip], transition_.fromTime, restPose_),
        AnimationSampler::sample(model_->animations()[transition_.toClip], 0, restPose_), 0));
}

void AnimationPlayer::stop(Scene &scene)
{
    apply(scene, restPose_);
    time_ = 0; selected_ = false; playing_ = false;
    transition_ = {};
}

void AnimationPlayer::seek(Scene &scene, double seconds)
{
    if (!selected_) { throw std::logic_error("Select an animation before seeking"); }
    if (!std::isfinite(seconds) || seconds < 0) { throw std::invalid_argument("Invalid animation seek time"); }
    const auto &clip = model_->animations()[clip_];
    const double target = std::min(seconds, static_cast<double>(clip.duration()));
    apply(scene, AnimationSampler::sample(clip, target, restPose_));
    time_ = target;
    transition_ = {};
}

void AnimationPlayer::update(Scene &scene, float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0) { throw std::invalid_argument("Invalid animation delta time"); }
    if (!playing_ || !selected_) { return; }
    const auto &clip = model_->animations()[clip_];
    if (transition_.active)
    {
        const auto advance = [&](std::size_t index, double current)
        {
            const double duration = model_->animations()[index].duration();
            const double next = current + static_cast<double>(deltaTime) * speed_;
            return duration <= 0 ? 0.0 : (looping_ ? std::fmod(next, duration) : std::min(next, duration));
        };
        transition_.fromTime = advance(transition_.fromClip, transition_.fromTime);
        const double previousToTime = transition_.toTime;
        transition_.toTime = advance(transition_.toClip, transition_.toTime);
        emitEvents(model_->animations()[transition_.toClip], previousToTime, transition_.toTime,
            looping_, eventCallback_);
        transition_.elapsed += deltaTime;
        const float weight = transition_.duration <= 0 ? 1.0f : static_cast<float>(std::min(1.0, transition_.elapsed / transition_.duration));
        apply(scene, AnimationBlender::blend(
            AnimationSampler::sample(model_->animations()[transition_.fromClip], transition_.fromTime, restPose_),
            AnimationSampler::sample(model_->animations()[transition_.toClip], transition_.toTime, restPose_), weight));
        if (weight >= 1.0f)
        {
            clip_ = transition_.toClip; time_ = transition_.toTime; transition_ = {};
        }
        return;
    }
    const double duration = clip.duration();
    // 用double保存时钟，大帧间隔/速度不会使float相乘溢出。单帧片段不存在取模分母。
    const double advanced = time_ + static_cast<double>(deltaTime) * speed_;
    const double target = duration <= 0 ? 0 : (looping_ ? std::fmod(advanced, duration) : std::min(advanced, duration));
    emitEvents(clip, time_, target, looping_, eventCallback_);
    apply(scene, AnimationSampler::sample(clip, target, restPose_));
    time_ = target;
    if (duration <= 0 || (!looping_ && advanced >= duration)) { playing_ = false; }
}

void AnimationPlayer::validateClipIndex(std::size_t clipIndex) const
{
    if (clipIndex >= model_->animations().size()) { throw std::out_of_range("Animation clip index is out of range"); }
}

void AnimationPlayer::setSpeed(float speed)
{
    if (!std::isfinite(speed) || speed < 0) { throw std::invalid_argument("Animation speed must be finite and nonnegative"); }
    speed_ = speed;
}
