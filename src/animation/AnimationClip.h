#pragma once

#include <glm/vec4.hpp>
#include <cstddef>
#include <string>
#include <vector>

enum class AnimationPath { Translation, Rotation, Scale };
enum class AnimationInterpolation { Step, Linear };

// 通道只绑定模型内的节点索引，不保存场景指针或节点名称。
// values的位置/缩放使用xyz，旋转使用xyzw；times单位为秒，必须严格递增。
struct AnimationChannel
{
    std::size_t node = 0;
    AnimationPath path = AnimationPath::Translation;
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    std::vector<float> times;
    std::vector<glm::vec4> values;
};

// 构造时一次性校验；构造后只读，多个播放器可安全共享同一份关键帧。
// 同一片段不能重复控制同一节点的同一属性，避免依赖通道顺序覆盖结果。
class AnimationClip final
{
public:
    AnimationClip(std::string name, std::vector<AnimationChannel> channels);
    const std::string &name() const noexcept { return name_; }
    const std::vector<AnimationChannel> &channels() const noexcept { return channels_; }
    float duration() const noexcept { return duration_; }
private:
    std::string name_;
    std::vector<AnimationChannel> channels_;
    float duration_ = 0;
};
