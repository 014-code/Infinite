#include "AnimationBlender.h"

#include <glm/geometric.hpp>
#include <cmath>
#include <stdexcept>

std::vector<LocalPose> AnimationBlender::blend(const std::vector<LocalPose> &from,
    const std::vector<LocalPose> &to, float weight)
{
    if (from.size() != to.size() || !std::isfinite(weight) || weight < 0 || weight > 1)
    { throw std::invalid_argument("Animation poses must have equal size and [0,1] weight"); }
    std::vector<LocalPose> result;
    result.reserve(from.size());
    for (std::size_t index = 0; index < from.size(); ++index)
    {
        auto rotation = to[index].rotation;
        if (glm::dot(from[index].rotation, rotation) < 0) { rotation = -rotation; }
        result.push_back({
            glm::mix(from[index].position, to[index].position, weight),
            glm::normalize(glm::slerp(from[index].rotation, rotation, weight)),
            glm::mix(from[index].scale, to[index].scale, weight)
        });
    }
    return result;
}
