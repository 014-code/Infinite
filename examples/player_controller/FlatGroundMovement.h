#pragma once

#include "PlayerCommand.h"

class Transform;

namespace PlayerExample
{
    struct FlatGroundSettings
    {
        float moveSpeed = 3.0f;
        float sprintMultiplier = 2.5f;
        float jumpSpeed = 5.0f;
        float gravity = -18.0f;
        float maxFallSpeed = 30.0f;
        float groundHeight = 0.0f;
    };

    // 仅供示例使用的无限平地移动规则，不是引擎角色物理组件。
    // groundHeight是Transform原点最低高度，不读取场景地板，也不检测墙、坡和台阶。
    // 一个实例保存一个角色的竖直状态，不应交替更新多个角色；仅支持根Transform。
    // 键盘、AI或测试都可以提交PlayerCommand；朝向、按键、失焦和重生位置由应用决定。
    class FlatGroundMovement final
    {
    public:
        explicit FlatGroundMovement(const FlatGroundSettings &settings = {});

        // 指令为空时仍处理重力。movement长度超过1时限幅，小于1时保留模拟输入强度。
        // deltaTime为0时不修改状态；jumpPressed仅表示一次请求，调用方不要每帧重复提交。
        void update(Transform &transform, const PlayerCommand &command, float deltaTime);

        // 只清除竖直状态并回到演示地面，保留X/Z、缩放与旋转。完整重生由main负责。
        void reset(Transform &transform);
        bool grounded() const noexcept;
        float verticalVelocity() const noexcept;

    private:
        FlatGroundSettings settings_;
        float verticalVelocity_ = 0.0f;
        // 首次正时长更新前尚未检查玩家位置，不假定玩家出生在地面。
        bool grounded_ = false;
    };
}
