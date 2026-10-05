#pragma once

// 单帧时间快照。真实时间用于统计；运动逻辑应使用deltaTime/simulationTime。
struct FrameTime
{
    float realDeltaTime = 0.0f;
    double realElapsedTime = 0.0;
    float deltaTime = 0.0f;
    double simulationTime = 0.0;
};

// 纯CPU时间策略，与具体时钟和窗口分离，测试不必真的等待数秒。
class FrameTiming
{
public:
    explicit FrameTiming(float maximumDeltaTime = 0.1f);

    // 输入上次采样后真实经过的秒数。paused表示本轮不更新、不绘制。
    // 首帧及暂停恢复首帧的运动dt为0，避免把初始化/暂停时间一次性补进游戏。
    void advance(float realDeltaTime, bool paused);
    const FrameTime &frame() const;

private:
    float maximumDeltaTime_;
    bool restart_ = true;
    FrameTime frame_;
};
