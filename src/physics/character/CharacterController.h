#pragma once

#include "physics/math/Ray.h"
#include "physics/shapes/CollisionShape.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstdint>

class PhysicsWorld;

// 角色控制器参数：把角色抽象为竖直胶囊（总高 = 2 * radius + cylinderHeight）。
struct CharacterSettings
{
    float radius = 0.35f;
    // 胶囊圆柱段高度，不含两端半球帽。总高 = cylinderHeight + 2 * radius。
    float cylinderHeight = 1.0f;
    // 皮肤宽度：解决穿透时额外推出的距离，避免角色与表面始终保持微小重叠而抖动。
    float skinWidth = 0.02f;
    // 可站立的最大斜面角；超过该角度的面按不可站立处理，角色会沿其下滑。
    float maximumSlopeAngleDegrees = 46.0f;
    // 每次移动子步中最多沿表面滑动几次；次数用尽后剩余位移被丢弃，不会穿墙。
    int maximumSlideIterations = 4;
    // 着地吸附距离：离地不超过该距离的可行走面会把角色重新贴回地面（下坡不会反复离地）。
    float groundSnapDistance = 0.08f;
};

// 角色的运动状态。velocity由调用方写入（含重力与跳跃），move返回修正后的结果。
struct CharacterState
{
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
    bool grounded = false;
    glm::vec3 groundNormal{0.0f, 1.0f, 0.0f};
};

// 角色移动规则：只依赖PhysicsWorld的静态体，不修改世界状态，因此可直接做无窗口单元测试。
//
// 每帧调用一次move：内部按子步前进，遇到碰撞时沿表面滑动并记录可站立面；不做真实碰撞
// 动力学，也不处理移动平台载人、台阶自动攀爬和角色之间的相互推挤。
// 重力与跳跃由调用方写入state.velocity，本类不内置按键或输入策略。
class CharacterController
{
public:
    explicit CharacterController(const CharacterSettings &settings = {});

    const CharacterSettings &settings() const noexcept { return settings_; }

    // 用deltaTime推进角色。dt为0时只做一次着地检测，不移动。
    // queryMask用于筛选角色能与哪些层的静态体碰撞，默认全部。
    CharacterState move(const PhysicsWorld &world, const CharacterState &current, float deltaTime,
        std::uint32_t queryMask = 0xFFFFFFFFu) const;

    // 覆盖角色的局部形状（世界空间胶囊，位置为角色原点）。
    CollisionShape shape() const;
    // 胶囊两端球心相对角色原点的偏移。
    float capCenterOffset() const;

private:
    CharacterSettings settings_;
    float walkableCos_ = 0.0f;
};
