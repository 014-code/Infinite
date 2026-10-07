#include "AnimationClip.h"
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

AnimationClip::AnimationClip(std::string name, std::vector<AnimationChannel> channels,
    std::vector<AnimationEvent> events)
    : name_(std::move(name)), channels_(std::move(channels)), events_(std::move(events))
{
    std::set<std::pair<std::size_t, AnimationPath>> targets;
    for (auto &channel : channels_)
    {
        if (channel.path != AnimationPath::Translation && channel.path != AnimationPath::Rotation &&
            channel.path != AnimationPath::Scale) { throw std::invalid_argument("Invalid animation path"); }
        if (channel.interpolation != AnimationInterpolation::Step &&
            channel.interpolation != AnimationInterpolation::Linear)
        { throw std::invalid_argument("Unsupported animation interpolation"); }
        if (!targets.emplace(channel.node, channel.path).second)
        { throw std::invalid_argument("Duplicate animation node/path channel"); }
        if (channel.times.empty() || channel.times.size() != channel.values.size())
        { throw std::invalid_argument("Animation key times/values must have equal positive counts"); }
        for (std::size_t key = 0; key < channel.times.size(); ++key)
        {
            const float time = channel.times[key];
            if (!std::isfinite(time) || time < 0 || (key > 0 && time <= channel.times[key - 1]))
            { throw std::invalid_argument("Animation times must be finite, nonnegative and strictly increasing"); }
            auto &value = channel.values[key];
            for (int component = 0; component < 4; ++component)
            {
                if (!std::isfinite(value[component]))
                { throw std::invalid_argument("Animation values must be finite"); }
            }
            if (channel.path == AnimationPath::Rotation)
            {
                const float length = glm::length(value);
                if (!std::isfinite(length) || std::abs(length - 1.0f) > .001f)
                { throw std::invalid_argument("Animation rotations must be unit quaternions"); }
                value /= length;
            }
        }
        duration_ = std::max(duration_, channel.times.back());
    }
    for (const auto &event : events_)
    {
        if (!std::isfinite(event.time) || event.time < 0.0 ||
            event.time > static_cast<double>(duration_) || event.name.empty())
        {
            throw std::invalid_argument("Animation events must have a valid time and name");
        }
    }
    std::stable_sort(events_.begin(), events_.end(),
        [](const AnimationEvent &left, const AnimationEvent &right)
        {
            return left.time < right.time;
        });
}
