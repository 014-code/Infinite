#pragma once

#include <chrono>

class Time
{
public:
    // 创建时间系统，并记录第一帧的起始时间
    Time();

    // 在每帧开始时更新帧间隔和程序运行时间
    void update();

    // 获取上一帧到当前帧经过的秒数
    float deltaTime() const;

    // 获取程序启动后经过的秒数
    float elapsedTime() const;

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point startTime_;
    Clock::time_point previousFrameTime_;
    float deltaTime_ = 0.0f;
    float elapsedTime_ = 0.0f;
};
