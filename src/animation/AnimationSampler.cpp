#include "AnimationSampler.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    glm::quat quaternion(const glm::vec4 &value)
    {
        // glTF/通道使用xyzw，GLM的构造顺序是wxyz。
        return {value.w, value.x, value.y, value.z};
    }

    glm::vec4 sampleChannel(const AnimationChannel &channel, double seconds)
    {
        const auto &times = channel.times;
        if (seconds <= times.front()) { return channel.values.front(); }
        if (seconds >= times.back()) { return channel.values.back(); }
        const auto right = static_cast<std::size_t>(std::upper_bound(times.begin(), times.end(), seconds) - times.begin());
        const auto left = right - 1;
        if (channel.interpolation == AnimationInterpolation::Step) { return channel.values[left]; }
        const float ratio = static_cast<float>((seconds - times[left]) / (times[right] - times[left]));
        if (channel.path == AnimationPath::Rotation)
        {
            const auto a = quaternion(channel.values[left]);
            auto b = quaternion(channel.values[right]);
            // q和-q表示相同旋转。先对齐符号，避免沿大圆的长路径转一整圈。
            if (glm::dot(a, b) < 0) { b = -b; }
            const auto result = glm::normalize(glm::slerp(a, b, ratio));
            return {result.x, result.y, result.z, result.w};
        }
        // 用double计算加权和，避免两个有限但很大的端点相减时发生float溢出。
        return glm::vec4(glm::dvec4(channel.values[left]) * (1.0 - ratio) +
            glm::dvec4(channel.values[right]) * static_cast<double>(ratio));
    }
}

std::vector<LocalPose> AnimationSampler::sample(const AnimationClip &clip, double seconds,
    const std::vector<LocalPose> &restPose)
{
    if (!std::isfinite(seconds) || seconds < 0) { throw std::invalid_argument("Animation time must be finite and nonnegative"); }
    auto result = restPose;
    for (const auto &channel : clip.channels())
    {
        if (channel.node >= result.size()) { throw std::out_of_range("Animation channel node is outside the pose"); }
        const auto value = sampleChannel(channel, seconds);
        auto &pose = result[channel.node];
        switch (channel.path)
        {
        case AnimationPath::Translation: pose.position = glm::vec3(value); break;
        case AnimationPath::Rotation: pose.rotation = quaternion(value); break;
        case AnimationPath::Scale: pose.scale = glm::vec3(value); break;
        }
    }
    return result;
}
