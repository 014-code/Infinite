#include "scene/components/CharacterBodyComponent.h"

#include "math/Transform.h"
#include "physics/world/PhysicsWorld.h"
#include "scene/components/PhysicsTransformRules.h"

#include <stdexcept>
#include <utility>

void CharacterBodyComponent::attach(PhysicsWorld &world, const CharacterSettings &settings,
    const glm::vec3 &position, std::uint32_t queryMask)
{
    // 先构造新的控制器，参数非法时不会破坏已经存在的角色绑定。
    CharacterController controller(settings);
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
    {
        throw std::invalid_argument("Character body position must be finite");
    }
    world_ = &world;
    controller_ = std::move(controller);
    state_ = CharacterState{};
    state_.position = position;
    queryMask_ = queryMask;
}

void CharacterBodyComponent::detach() noexcept
{
    world_ = nullptr;
    state_ = CharacterState{};
    queryMask_ = 0xFFFFFFFFu;
}

bool CharacterBodyComponent::isAttached() const noexcept
{
    return world_ != nullptr;
}

const CharacterSettings &CharacterBodyComponent::settings() const noexcept
{
    return controller_.settings();
}

CharacterController &CharacterBodyComponent::controller() noexcept
{
    return controller_;
}

const CharacterController &CharacterBodyComponent::controller() const noexcept
{
    return controller_;
}

CharacterState &CharacterBodyComponent::state() noexcept
{
    return state_;
}

const CharacterState &CharacterBodyComponent::state() const noexcept
{
    return state_;
}

void CharacterBodyComponent::setVelocity(const glm::vec3 &velocity)
{
    if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y) || !std::isfinite(velocity.z))
    {
        throw std::invalid_argument("Character body velocity must be finite");
    }
    state_.velocity = velocity;
}

void CharacterBodyComponent::setQueryMask(std::uint32_t queryMask) noexcept
{
    queryMask_ = queryMask;
}

std::uint32_t CharacterBodyComponent::queryMask() const noexcept
{
    return queryMask_;
}

void CharacterBodyComponent::move(float fixedDeltaTime)
{
    if (world_ == nullptr)
    {
        throw std::logic_error("Character body is not attached to a PhysicsWorld");
    }
    state_ = controller_.move(*world_, state_, fixedDeltaTime, queryMask_);
}

bool CharacterBodyComponent::syncToTransform(Transform &transform) const
{
    if (!isAttached())
    {
        return false;
    }
    if (transform.parent() != nullptr)
    {
        throw std::invalid_argument("Character body Transform must be a root Transform");
    }
    PhysicsTransformRules::validate(transform);
    transform.position = state_.position;
    return true;
}
