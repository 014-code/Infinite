#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

// 刚体参数。首版只支持球体动态体（见PhysicsWorld::createDynamicBody的限制说明）。
struct RigidBodySettings
{
    float mass = 1.0f;
    // 恢复系数：0为完全非弹性，1为完全弹性。碰撞时取两侧的较大值。
    float restitution = 0.25f;
    // 库仑摩擦系数，作用于接触切向速度。
    float friction = 0.4f;
    // 每秒线性速度衰减比例，避免长时间漂浮抖动。
    float linearDamping = 0.02f;
    // 角速度衰减比例；首版连杆不产生力矩，角速度只由应用或衰减改变。
    float angularDamping = 0.05f;
    // 低速持续超过sleepTime秒后进入休眠；休眠体不参与积分与接触求解。
    bool allowSleep = true;
    float sleepVelocityThreshold = 0.06f;
    float sleepTime = 0.5f;
};

// 动态体的可读状态；渲染与查询使用，不直接暴露内部积分数据。
struct RigidBodyState
{
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    bool sleeping = false;
};
