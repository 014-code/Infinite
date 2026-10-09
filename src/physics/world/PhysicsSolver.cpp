#include "PhysicsSolver.h"

#include "physics/world/PhysicsQueries.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float POSITION_SLOP = 0.001f;
    // 位置修正比例：低于1可以避免多个接触互相抢修造成抖动，代价是每步残留少量穿透。
    constexpr float POSITION_CORRECTION = 0.8f;
    // 恢复系数速度阈值：接近速度低于该值时不弹跳，否则靠重力压出的微小穿透会让
    // 静止物体永远以微小速度抖动，无法进入休眠。
    constexpr float RESTITUTION_VELOCITY_THRESHOLD = 1.0f;
    constexpr float SLEEP_ANGULAR_THRESHOLD = 0.2f;
}

void PhysicsSolver::step(PhysicsWorld &world, float stepSeconds)
{
    // 1. 积分：只有未休眠的动态体参与。
    for (auto &body : world.bodies_)
    {
        if (!body->dynamic)
        {
            continue;
        }
        body->previousPosition = body->position;
        body->previousRotation = body->rotation;
        if (body->sleeping)
        {
            body->force = glm::vec3(0.0f);
            continue;
        }
        body->velocity += (world.gravity_ + body->force / body->settings.mass) * stepSeconds;
        // 力是固定步级别的累加量，不会意外跨帧残留；需要持续施力的应用应每步重新调用。
        body->force = glm::vec3(0.0f);
        body->velocity *= std::max(0.0f, 1.0f - body->settings.linearDamping * stepSeconds);
        body->angularVelocity *= std::max(0.0f, 1.0f - body->settings.angularDamping * stepSeconds);
        body->position += body->velocity * stepSeconds;
        const glm::vec3 angular = body->angularVelocity * stepSeconds;
        const float angularLength = glm::length(angular);
        if (angularLength > 0.0f)
        {
            // 小角度近似：用半角构造增量四元数，避免每步都做完整旋转矩阵运算。
            const glm::quat delta(std::cos(angularLength * 0.5f),
                angular * (std::sin(angularLength * 0.5f) / angularLength));
            body->rotation = glm::normalize(body->rotation * delta);
        }
    }

    // 2. 接触收集与求解。
    std::vector<Contact> contacts;
    collectContacts(world, contacts);
    if (!contacts.empty())
    {
        solveVelocity(world, contacts);
        correctPositions(world, contacts);
    }

    // 3. 休眠判定与包围盒刷新。
    for (auto &body : world.bodies_)
    {
        if (body->dynamic && !body->sleeping && body->settings.allowSleep)
        {
            const bool slow = glm::length(body->velocity) < body->settings.sleepVelocityThreshold &&
                glm::length(body->angularVelocity) < SLEEP_ANGULAR_THRESHOLD;
            body->sleepTimer = slow ? body->sleepTimer + stepSeconds : 0.0f;
            if (body->sleepTimer >= body->settings.sleepTime)
            {
                body->sleeping = true;
                body->velocity = glm::vec3(0.0f);
                body->angularVelocity = glm::vec3(0.0f);
            }
        }
        world.refreshWorldAabb(*body);
    }
    ++world.fixedStepCount_;
}

void PhysicsSolver::collectContacts(const PhysicsWorld &world, std::vector<Contact> &contacts)
{
    // 只有至少一侧是“醒着的动态体”时，接触才可能改变状态：
    // 静态-静态、休眠动态-静态、休眠-休眠都不必进入窄相位。
    const auto awakeDynamic = [&world](std::size_t index)
    {
        const auto &body = *world.bodies_[index];
        return body.dynamic && !body.sleeping;
    };

    // 有限体之间用扫掠剪枝候选对，避免对所有body做O(n²)全配对。
    std::vector<std::pair<std::size_t, std::size_t>> candidates;
    PhysicsQueries::collectSweepCandidates(world, candidates);

    std::size_t pairCount = 0;
    for (const auto &candidate : candidates)
    {
        const std::size_t first = candidate.first;
        const std::size_t second = candidate.second;
        if (!awakeDynamic(first) && !awakeDynamic(second))
        {
            continue;
        }
        // buildContact约定第一个下标是动态体，法线方向才是“把动态体推开”。
        if (world.bodies_[first]->dynamic)
        {
            buildContact(world, first, second, contacts);
        }
        else
        {
            buildContact(world, second, first, contacts);
        }
        ++pairCount;
    }

    // 平面没有有限包围盒，无法参与扫掠；平面数量很少，直接与每个醒着的动态体配对。
    for (std::size_t planeIndex = 0; planeIndex < world.bodies_.size(); ++planeIndex)
    {
        if (!world.bodies_[planeIndex]->shape.holds<PlaneShape>())
        {
            continue;
        }
        for (std::size_t dynamicIndex = 0; dynamicIndex < world.bodies_.size(); ++dynamicIndex)
        {
            if (dynamicIndex == planeIndex || !awakeDynamic(dynamicIndex))
            {
                continue;
            }
            if (!filtersInteract(world.bodies_[dynamicIndex]->filter,
                world.bodies_[planeIndex]->filter))
            {
                continue;
            }
            buildContact(world, dynamicIndex, planeIndex, contacts);
            ++pairCount;
        }
    }
    world.lastNarrowphasePairCount_ = pairCount;
}

void PhysicsSolver::buildContact(const PhysicsWorld &world, std::size_t indexA,
    std::size_t indexB, std::vector<Contact> &contacts)
{
    const auto &a = *world.bodies_[indexA];
    const auto &b = *world.bodies_[indexB];
    const std::optional<ShapeContact> contact = ShapeCollision::collide(a.shape, a.position, a.rotation,
        b.shape, b.position, b.rotation);
    if (!contact)
    {
        return;
    }

    // ShapeCollision的法线约定：平面参与时是平面正侧法线，其余是“A指向B”。
    // separationDirection把两种约定统一成“把动态体沿+normal推开”的分离方向。
    const glm::vec3 separation = ShapeCollision::separationDirection(*contact,
        b.shape.holds<PlaneShape>());

    Contact result;
    result.a = indexA;
    result.b = b.dynamic ? indexB : kNoBody;
    result.point = contact->point;
    result.normal = separation;
    result.penetration = contact->penetration;
    // 恢复系数取两侧较大值；摩擦取几何平均，避免单侧为0时完全无摩擦或完全黏住。
    const float restitutionA = a.settings.restitution;
    const float restitutionB = b.dynamic ? b.settings.restitution : 0.0f;
    result.restitution = std::max(restitutionA, restitutionB);
    const float frictionA = a.settings.friction;
    const float frictionB = b.dynamic ? b.settings.friction : 1.0f;
    result.friction = std::sqrt(std::max(0.0f, frictionA * frictionB));
    contacts.push_back(result);
}

void PhysicsSolver::solveVelocity(PhysicsWorld &world, std::vector<Contact> &contacts)
{
    for (int iteration = 0; iteration < world.solverIterations_; ++iteration)
    {
        for (Contact &contact : contacts)
        {
            auto &a = *world.bodies_[contact.a];
            auto &b = contact.b == kNoBody ? a : *world.bodies_[contact.b];
            const bool hasB = contact.b != kNoBody;

            // 唤醒被运动物体撞到的休眠体。
            if (a.sleeping && hasB && !b.sleeping)
            {
                a.sleeping = false;
                a.sleepTimer = 0.0f;
            }
            if (hasB && b.sleeping && !a.sleeping)
            {
                b.sleeping = false;
                b.sleepTimer = 0.0f;
            }
            const bool aActive = !a.sleeping;
            const bool bActive = !hasB || !b.sleeping;
            if (!aActive && !bActive)
            {
                continue;
            }

            const float inverseMassA = aActive ? 1.0f / a.settings.mass : 0.0f;
            const float inverseMassB = (hasB && bActive) ? 1.0f / b.settings.mass : 0.0f;
            const float inverseMassSum = inverseMassA + inverseMassB;
            if (inverseMassSum <= 0.0f)
            {
                continue;
            }

            const glm::vec3 relativeVelocity = a.velocity - (hasB ? b.velocity : glm::vec3(0.0f));
            const float normalVelocity = glm::dot(relativeVelocity, contact.normal);
            if (normalVelocity < 0.0f)
            {
                // 只有接触确实发生且接近速度足够大时才应用恢复系数，避免静止体持续微弹。
                const float restitution = (contact.penetration > POSITION_SLOP &&
                    -normalVelocity > RESTITUTION_VELOCITY_THRESHOLD) ? contact.restitution : 0.0f;
                const float impulseMagnitude = -(1.0f + restitution) * normalVelocity / inverseMassSum;
                const glm::vec3 impulse = contact.normal * impulseMagnitude;
                if (aActive)
                {
                    a.velocity += impulse * inverseMassA;
                }
                if (hasB && bActive)
                {
                    b.velocity -= impulse * inverseMassB;
                }

                // 切向摩擦冲量：限制在库仑摩擦锥内。
                const glm::vec3 afterRelative = a.velocity - (hasB ? b.velocity : glm::vec3(0.0f));
                const float afterNormal = glm::dot(afterRelative, contact.normal);
                const glm::vec3 tangent = afterRelative - contact.normal * afterNormal;
                const float tangentLength = glm::length(tangent);
                if (tangentLength > 1e-5f && contact.friction > 0.0f)
                {
                    const glm::vec3 tangentDirection = tangent / tangentLength;
                    // 摩擦冲量抵抗切向相对速度，并限制在库仑摩擦锥内。
                    float frictionMagnitude = -glm::dot(afterRelative, tangentDirection) / inverseMassSum;
                    const float limit = contact.friction * impulseMagnitude;
                    frictionMagnitude = std::clamp(frictionMagnitude, -limit, limit);
                    const glm::vec3 frictionImpulse = tangentDirection * frictionMagnitude;
                    if (aActive)
                    {
                        a.velocity += frictionImpulse * inverseMassA;
                    }
                    if (hasB && bActive)
                    {
                        b.velocity -= frictionImpulse * inverseMassB;
                    }
                }
            }
        }
    }
}

void PhysicsSolver::correctPositions(PhysicsWorld &world, std::vector<Contact> &contacts)
{
    // 位置修正独立于速度求解：直接按穿透深度把动态体推开，带slop避免抖动。
    for (const Contact &contact : contacts)
    {
        auto &a = *world.bodies_[contact.a];
        const bool hasB = contact.b != kNoBody;
        auto *b = hasB ? world.bodies_[contact.b].get() : nullptr;
        const bool aActive = !a.sleeping;
        const bool bActive = b != nullptr && !b->sleeping;
        if (!aActive && !bActive)
        {
            continue;
        }
        const float inverseMassA = (a.dynamic && aActive) ? 1.0f / a.settings.mass : 0.0f;
        const float inverseMassB = (b != nullptr && bActive) ? 1.0f / b->settings.mass : 0.0f;
        const float inverseMassSum = inverseMassA + inverseMassB;
        if (inverseMassSum <= 0.0f)
        {
            continue;
        }
        const float depth = contact.penetration - POSITION_SLOP;
        if (depth <= 0.0f)
        {
            continue;
        }
        const glm::vec3 correction = contact.normal * (depth * POSITION_CORRECTION / inverseMassSum);
        if (inverseMassA > 0.0f)
        {
            a.position += correction * inverseMassA;
            // 位置被推开后速度沿法线的分离分量保持不变，避免贴墙时的额外能量。
            const float normalVelocity = glm::dot(a.velocity, contact.normal);
            if (normalVelocity < 0.0f)
            {
                a.velocity -= contact.normal * normalVelocity;
            }
        }
        if (b != nullptr && inverseMassB > 0.0f)
        {
            b->position -= correction * inverseMassB;
        }
    }
}
