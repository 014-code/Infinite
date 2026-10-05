#include "physics/character/CharacterController.h"

#include "physics/world/PhysicsWorld.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    constexpr float EPSILON = 1e-5f;
    const glm::vec3 UP(0.0f, 1.0f, 0.0f);
    const glm::quat NO_ROTATION(1.0f, 0.0f, 0.0f, 0.0f);

    bool isFinite(const glm::vec3 &value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    // 找到穿透最深、且方向朝角色外侧的接触，用于着地吸附判定。
    const PhysicsWorld::OverlapResult *deepestWalkable(const std::vector<PhysicsWorld::OverlapResult> &overlaps,
        float walkableCos)
    {
        const PhysicsWorld::OverlapResult *best = nullptr;
        for (const PhysicsWorld::OverlapResult &overlap : overlaps)
        {
            if (glm::dot(overlap.pushOutNormal, UP) < walkableCos)
            {
                continue;
            }
            if (best == nullptr || overlap.contact.penetration > best->contact.penetration)
            {
                best = &overlap;
            }
        }
        return best;
    }
}

CharacterController::CharacterController(const CharacterSettings &settings)
    : settings_(settings)
{
    if (!std::isfinite(settings.radius) || settings.radius <= 0.0f)
    {
        throw std::invalid_argument("Character radius must be positive and finite");
    }
    if (!std::isfinite(settings.cylinderHeight) || settings.cylinderHeight < 0.0f)
    {
        throw std::invalid_argument("Character cylinder height must not be negative");
    }
    if (!std::isfinite(settings.skinWidth) || settings.skinWidth <= 0.0f)
    {
        throw std::invalid_argument("Character skin width must be positive and finite");
    }
    if (!std::isfinite(settings.maximumSlopeAngleDegrees) || settings.maximumSlopeAngleDegrees < 0.0f ||
        settings.maximumSlopeAngleDegrees >= 90.0f)
    {
        throw std::invalid_argument("Character maximum slope angle must be in [0, 90) degrees");
    }
    if (settings.maximumSlideIterations < 1 || settings.maximumSlideIterations > 16)
    {
        throw std::invalid_argument("Character slide iterations must be between 1 and 16");
    }
    if (!std::isfinite(settings.groundSnapDistance) || settings.groundSnapDistance < 0.0f)
    {
        throw std::invalid_argument("Character ground snap distance must not be negative");
    }
    walkableCos_ = std::cos(glm::radians(settings_.maximumSlopeAngleDegrees));
}

CollisionShape CharacterController::shape() const
{
    return CollisionShape(CapsuleShape(settings_.radius, settings_.cylinderHeight));
}

float CharacterController::capCenterOffset() const
{
    return settings_.cylinderHeight * 0.5f;
}

CharacterState CharacterController::move(const PhysicsWorld &world, const CharacterState &current, float deltaTime,
    std::uint32_t queryMask) const
{
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
    {
        throw std::invalid_argument("Character delta time must be non-negative and finite");
    }
    if (!isFinite(current.position) || !isFinite(current.velocity))
    {
        throw std::invalid_argument("Character state must be finite");
    }

    CharacterState state = current;
    const CollisionShape capsule = shape();
    const glm::vec3 displacement = current.velocity * deltaTime;

    // 子步：单个子步不超过半径的一半，避免高速穿过薄墙而没有任何一次重叠查询发现它。
    const float maximumSubStep = std::max(0.02f, settings_.radius * 0.5f);
    const float travelDistance = glm::length(displacement);
    int subSteps = travelDistance > maximumSubStep
        ? static_cast<int>(std::ceil(travelDistance / maximumSubStep))
        : 1;
    subSteps = std::clamp(subSteps, 1, 8);
    const glm::vec3 subStepDisplacement = displacement / static_cast<float>(subSteps);

    state.grounded = false;
    state.groundNormal = UP;

    for (int subStep = 0; subStep < subSteps; ++subStep)
    {
        glm::vec3 remaining = subStepDisplacement;
        for (int iteration = 0; iteration < settings_.maximumSlideIterations; ++iteration)
        {
            if (glm::length(remaining) <= EPSILON)
            {
                break;
            }
            const glm::vec3 candidate = state.position + remaining;
            const std::vector<PhysicsWorld::OverlapResult> overlaps = world.overlapShape(capsule, candidate,
                NO_ROTATION, queryMask);
            if (overlaps.empty())
            {
                state.position = candidate;
                remaining = glm::vec3(0.0f);
                break;
            }
            // 先把位置推进到候选点，再从候选点解决穿透；否则位移会被推出操作丢掉，
            // 表现为角色停在半空中不断"着地"。
            state.position = candidate;

            // 先解决最深的一次穿透，再把剩余位移与速度投影到接触平面，实现"沿表面滑动"。
            const PhysicsWorld::OverlapResult *deepest = nullptr;
            for (const PhysicsWorld::OverlapResult &overlap : overlaps)
            {
                if (deepest == nullptr || overlap.contact.penetration > deepest->contact.penetration)
                {
                    deepest = &overlap;
                }
            }
            // 贴着表面时只需要推出皮肤宽度的一半，避免在斜坡上被反复弹开。
            const float pushOut = deepest->contact.penetration + settings_.skinWidth * 0.5f;
            state.position += deepest->pushOutNormal * pushOut;

            for (const PhysicsWorld::OverlapResult &overlap : overlaps)
            {
                const glm::vec3 &normal = overlap.pushOutNormal;
                const float displacementIntoSurface = glm::dot(remaining, normal);
                if (displacementIntoSurface < 0.0f)
                {
                    remaining -= normal * displacementIntoSurface;
                }
                const float velocityIntoSurface = glm::dot(state.velocity, normal);
                if (velocityIntoSurface < 0.0f)
                {
                    state.velocity -= normal * velocityIntoSurface;
                }
                if (glm::dot(normal, UP) >= walkableCos_)
                {
                    state.grounded = true;
                    state.groundNormal = normal;
                }
            }
        }
    }

    // 着地吸附：离地很近的可行走面把角色贴回地面，避免下坡时反复腾空。
    if (!state.grounded && settings_.groundSnapDistance > 0.0f && state.velocity.y <= 0.0f)
    {
        const glm::vec3 probePosition = state.position - UP * settings_.groundSnapDistance;
        const std::vector<PhysicsWorld::OverlapResult> groundContacts = world.overlapShape(capsule, probePosition,
            NO_ROTATION, queryMask);
        const PhysicsWorld::OverlapResult *ground = deepestWalkable(groundContacts, walkableCos_);
        if (ground != nullptr)
        {
            state.position = probePosition + ground->pushOutNormal *
                (ground->contact.penetration + settings_.skinWidth * 0.5f);
            state.grounded = true;
            state.groundNormal = ground->pushOutNormal;
        }
    }
    // 斜面接触可能给出轻微倾斜的法线；退化情况下回落到世界向上方向。
    if (state.grounded && glm::length(state.groundNormal) < 0.5f)
    {
        state.groundNormal = UP;
    }

    if (state.grounded && state.velocity.y < 0.0f)
    {
        // 落地后不再积累向下速度；跳跃由调用方直接写入正的velocity.y。
        state.velocity.y = 0.0f;
    }
    if (!isFinite(state.position) || !isFinite(state.velocity))
    {
        throw std::runtime_error("Character movement produced a non-finite state");
    }
    return state;
}
