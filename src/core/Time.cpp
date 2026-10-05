#include "Time.h"

Time::Time()
    : startTime_(Clock::now()), previousFrameTime_(startTime_)
{
}

// 用单调时钟计算经过时间，避免系统时间调整影响帧间隔
void Time::update()
{
    const Clock::time_point currentTime = Clock::now();
    deltaTime_ = std::chrono::duration<float>(currentTime - previousFrameTime_).count();
    elapsedTime_ = std::chrono::duration<float>(currentTime - startTime_).count();
    previousFrameTime_ = currentTime;
}

float Time::deltaTime() const
{
    return deltaTime_;
}

float Time::elapsedTime() const
{
    return elapsedTime_;
}
