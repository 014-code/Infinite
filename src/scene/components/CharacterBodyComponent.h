#pragma once

#include "physics/character/CharacterController.h"
#include "scene/components/Component.h"

#include <cstdint>

class PhysicsWorld;
class Transform;

// CharacterBodyComponent把CharacterController的运行状态绑定到一个GameObject。
//
// 输入、摄像机相对方向和跳跃规则仍属于应用层；组件只负责保存角色状态，
// 并要求调用方在PhysicsWorld的固定步回调中调用move()。这样角色和刚体使用同一
// 时间基准，同时不会把某一种游戏操作方案硬编码进引擎。
class CharacterBodyComponent : public Component
{
public:
    CharacterBodyComponent() = default;
    explicit CharacterBodyComponent(GameObject &owner) noexcept : Component(owner) {}
    ~CharacterBodyComponent() override = default;
    CharacterBodyComponent(const CharacterBodyComponent &) = delete;
    CharacterBodyComponent &operator=(const CharacterBodyComponent &) = delete;
    CharacterBodyComponent(CharacterBodyComponent &&) = delete;
    CharacterBodyComponent &operator=(CharacterBodyComponent &&) = delete;

    // 绑定物理世界和角色形状。角色本身不是PhysicsWorld中的RigidBody，
    // 而是通过overlapShape查询静态碰撞体，因此不会被动态刚体自动推动。
    void attach(PhysicsWorld &world, const CharacterSettings &settings = {},
        const glm::vec3 &position = glm::vec3(0.0f),
        std::uint32_t queryMask = 0xFFFFFFFFu);
    void detach() noexcept;

    bool isAttached() const noexcept;
    const CharacterSettings &settings() const noexcept;
    CharacterController &controller() noexcept;
    const CharacterController &controller() const noexcept;

    // 状态由组件持有，输入层只需要修改velocity或读取grounded。
    CharacterState &state() noexcept;
    const CharacterState &state() const noexcept;
    void setVelocity(const glm::vec3 &velocity);
    void setQueryMask(std::uint32_t queryMask) noexcept;
    std::uint32_t queryMask() const noexcept;

    // 在固定步回调中推进角色。重力和跳跃仍由调用方写入state.velocity，
    // 这样组件可以用于不同的游戏规则，而不会强制所有角色使用同一种重力。
    void move(float fixedDeltaTime);

    // 物理只写位置，不覆盖应用层控制的朝向；返回值表示当前组件仍然有效。
    bool syncToTransform(Transform &transform) const;

private:
    PhysicsWorld *world_ = nullptr;
    CharacterController controller_;
    CharacterState state_;
    std::uint32_t queryMask_ = 0xFFFFFFFFu;
};
