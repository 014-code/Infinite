#include "FrameTiming.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

FrameTiming::FrameTiming(float maximumDeltaTime)
    : maximumDeltaTime_(maximumDeltaTime)
{
    if (!std::isfinite(maximumDeltaTime) || maximumDeltaTime <= 0.0f)
    {
        throw std::invalid_argument("Maximum frame delta must be positive and finite");
    }
}

void FrameTiming::advance(float realDeltaTime, bool paused)
{
    if (!std::isfinite(realDeltaTime) || realDeltaTime < 0.0f)
    {
        throw std::invalid_argument("Frame delta must be nonnegative and finite");
    }

    frame_.realDeltaTime = realDeltaTime;
    frame_.realElapsedTime += realDeltaTime;
    frame_.deltaTime = paused || restart_ ? 0.0f : std::min(realDeltaTime, maximumDeltaTime_);
    frame_.simulationTime += frame_.deltaTime;
    // 连续暂停都保持重启标记，直到下一次可绘制帧才消费它。
    restart_ = paused;
}

const FrameTime &FrameTiming::frame() const
{
    return frame_;
}
