#include "physics/world/PhysicsWorld.h"

#include "physics/world/PhysicsQueries.h"
#include "physics/world/PhysicsSolver.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    bool isFinite(const glm::vec3 &value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

}

PhysicsWorld::PhysicsWorld() = default;

void PhysicsWorld::setGravity(const glm::vec3 &gravity)
{
    if (!isFinite(gravity))
    {
        throw std::invalid_argument("Physics gravity must be finite");
    }
    gravity_ = gravity;
}

const glm::vec3 &PhysicsWorld::gravity() const noexcept
{
    return gravity_;
}

void PhysicsWorld::setFixedStep(float seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0f)
    {
        throw std::invalid_argument("Physics fixed step must be positive and finite");
    }
    fixedStep_ = seconds;
    accumulator_ = 0.0f;
}

float PhysicsWorld::fixedStep() const noexcept
{
    return fixedStep_;
}

void PhysicsWorld::setMaximumSubSteps(int steps)
{
    if (steps < 1 || steps > 32)
    {
        throw std::invalid_argument("Physics maximum sub steps must be between 1 and 32");
    }
    maximumSubSteps_ = steps;
}

int PhysicsWorld::maximumSubSteps() const noexcept
{
    return maximumSubSteps_;
}

void PhysicsWorld::setSolverIterations(int iterations)
{
    if (iterations < 1 || iterations > 64)
    {
        throw std::invalid_argument("Physics solver iterations must be between 1 and 64");
    }
    solverIterations_ = iterations;
}

int PhysicsWorld::solverIterations() const noexcept
{
    return solverIterations_;
}

int PhysicsWorld::step(float frameDeltaTime)
{
    return step(frameDeltaTime, FixedStepCallback{});
}

int PhysicsWorld::step(float frameDeltaTime, const FixedStepCallback &callback)
{
    if (!std::isfinite(frameDeltaTime) || frameDeltaTime < 0.0f)
    {
        throw std::invalid_argument("Physics frame delta time must be non-negative and finite");
    }
    accumulator_ += frameDeltaTime;
    int steps = 0;
    while (accumulator_ >= fixedStep_ && steps < maximumSubSteps_)
    {
        if (callback)
        {
            callback(fixedStep_);
        }
        PhysicsSolver::step(*this, fixedStep_);
        accumulator_ -= fixedStep_;
        ++steps;
    }
    if (accumulator_ >= fixedStep_)
    {
        // 达到补跑上限：丢弃剩余时间，避免卡顿后一次补跑大量物理步。
        accumulator_ = 0.0f;
    }
    return steps;
}

void PhysicsWorld::resetAccumulator()
{
    accumulator_ = 0.0f;
}

float PhysicsWorld::interpolationAlpha() const noexcept
{
    return std::clamp(accumulator_ / fixedStep_, 0.0f, 1.0f);
}

std::uint64_t PhysicsWorld::fixedStepCount() const noexcept
{
    return fixedStepCount_;
}

std::size_t PhysicsWorld::lastNarrowphasePairCount() const noexcept
{
    return lastNarrowphasePairCount_;
}

PhysicsBodyId PhysicsWorld::createStaticBody(const CollisionShape &shape, const glm::vec3 &position)
{
    return createStaticBody(shape, position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
}

PhysicsBodyId PhysicsWorld::createStaticBody(const CollisionShape &shape, const glm::vec3 &position,
    const glm::quat &rotation, const PhysicsFilter &filter)
{
    auto body = std::make_unique<Body>(nextId_++, shape, filter);
    body->position = position;
    // 先归一化并校验旋转：零四元数或非有限值在这里就抛，不会先产生NaN。
    body->rotation = ShapeCollision::normalizedRotation(rotation);
    body->previousPosition = body->position;
    body->previousRotation = body->rotation;
    // 位置非法同样会在composeTransform内抛出，避免世界中出现半有效状态。
    refreshWorldAabb(*body);
    const PhysicsBodyId id = body->id;
    bodies_.push_back(std::move(body));
    const std::size_t index = bodies_.size() - 1;
    std::list<PhysicsBodyId>::iterator orderPosition;
    try
    {
        orderPosition = registrationOrder_.insert(registrationOrder_.end(), id);
        indexById_.emplace(id, index);
        orderById_.emplace(id, orderPosition);
    }
    catch (...)
    {
        indexById_.erase(id);
        orderById_.erase(id);
        if (!registrationOrder_.empty() && registrationOrder_.back() == id)
        {
            registrationOrder_.pop_back();
        }
        bodies_.pop_back();
        throw;
    }
    return id;
}

PhysicsBodyId PhysicsWorld::createDynamicBody(const CollisionShape &shape, const glm::vec3 &position,
    const RigidBodySettings &settings, const PhysicsFilter &filter)
{
    if (!shape.holds<SphereShape>())
    {
        // 盒体与胶囊的动态碰撞需要OBB对OBB/胶囊对盒等窄相位，首版没有实现；
        // 明确拒绝而不是用一个近似形状冒充。
        throw std::invalid_argument("Dynamic bodies currently support SphereShape only");
    }
    if (settings.mass <= 0.0f || !std::isfinite(settings.mass) ||
        settings.restitution < 0.0f || settings.restitution > 1.0f || !std::isfinite(settings.restitution) ||
        settings.friction < 0.0f || !std::isfinite(settings.friction) ||
        settings.linearDamping < 0.0f || settings.linearDamping > 1.0f || !std::isfinite(settings.linearDamping) ||
        settings.angularDamping < 0.0f || settings.angularDamping > 1.0f || !std::isfinite(settings.angularDamping) ||
        settings.sleepTime < 0.0f || !std::isfinite(settings.sleepTime) ||
        settings.sleepVelocityThreshold < 0.0f || !std::isfinite(settings.sleepVelocityThreshold))
    {
        throw std::invalid_argument("Rigid body settings are invalid");
    }

    const PhysicsBodyId id = createStaticBody(shape, position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), filter);
    Body *body = find(id);
    body->dynamic = true;
    body->settings = settings;
    return id;
}

bool PhysicsWorld::destroyBody(PhysicsBodyId id)
{
    const auto entry = indexById_.find(id);
    if (entry == indexById_.end())
    {
        return false;
    }
    const auto order = orderById_.find(id);
    if (order == orderById_.end())
    {
        throw std::logic_error("Physics body registration order is inconsistent");
    }

    const std::size_t removedIndex = entry->second;
    const std::size_t lastIndex = bodies_.size() - 1;
    if (removedIndex != lastIndex)
    {
        // unique_ptr转移不会移动Body本身的地址；只更新被搬到空位的那个ID。
        bodies_[removedIndex] = std::move(bodies_[lastIndex]);
        indexById_[bodies_[removedIndex]->id] = removedIndex;
    }
    bodies_.pop_back();

    registrationOrder_.erase(order->second);
    orderById_.erase(order);
    indexById_.erase(id);
    return true;
}

void PhysicsWorld::clear()
{
    bodies_.clear();
    indexById_.clear();
    registrationOrder_.clear();
    orderById_.clear();
    accumulator_ = 0.0f;
    lastNarrowphasePairCount_ = 0;
}

bool PhysicsWorld::contains(PhysicsBodyId id) const
{
    return find(id) != nullptr;
}

std::size_t PhysicsWorld::bodyCount() const
{
    return bodies_.size();
}

std::vector<PhysicsBodyId> PhysicsWorld::bodyIds() const
{
    std::vector<PhysicsBodyId> ids;
    ids.reserve(registrationOrder_.size());
    for (const PhysicsBodyId id : registrationOrder_)
    {
        ids.push_back(id);
    }
    return ids;
}

std::size_t PhysicsWorld::dynamicBodyCount() const
{
    return static_cast<std::size_t>(std::count_if(bodies_.begin(), bodies_.end(),
        [](const std::unique_ptr<Body> &body) { return body->dynamic; }));
}

bool PhysicsWorld::setBodyTransform(PhysicsBodyId id, const glm::vec3 &position, const glm::quat &rotation)
{
    Body *body = find(id);
    if (body == nullptr)
    {
        return false;
    }
    // 先完整校验新位姿，再写入Body。这样调用方传入NaN或非法四元数时，
    // 不会出现“函数抛异常但旧碰撞体已经被部分改写”的半更新状态。
    const glm::quat normalizedRotation = ShapeCollision::normalizedRotation(rotation);
    ShapeCollision::composeTransform(position, normalizedRotation);
    body->position = position;
    body->rotation = normalizedRotation;
    if (body->dynamic)
    {
        // 瞬移后不保留旧速度与休眠状态，否则下一步会立刻把物体拉回去。
        body->velocity = glm::vec3(0.0f);
        body->angularVelocity = glm::vec3(0.0f);
        body->force = glm::vec3(0.0f);
        body->sleeping = false;
        body->sleepTimer = 0.0f;
    }
    body->previousPosition = body->position;
    body->previousRotation = body->rotation;
    refreshWorldAabb(*body);
    return true;
}

std::optional<glm::vec3> PhysicsWorld::bodyPosition(PhysicsBodyId id) const
{
    const Body *body = find(id);
    if (body == nullptr)
    {
        return std::nullopt;
    }
    return body->position;
}

std::optional<glm::quat> PhysicsWorld::bodyRotation(PhysicsBodyId id) const
{
    const Body *body = find(id);
    if (body == nullptr)
    {
        return std::nullopt;
    }
    return body->rotation;
}

bool PhysicsWorld::isDynamicBody(PhysicsBodyId id) const
{
    const Body *body = find(id);
    return body != nullptr && body->dynamic;
}

const CollisionShape *PhysicsWorld::bodyShape(PhysicsBodyId id) const
{
    const Body *body = find(id);
    return body == nullptr ? nullptr : &body->shape;
}

std::uint32_t PhysicsWorld::bodyLayer(PhysicsBodyId id) const
{
    const Body *body = find(id);
    return body == nullptr ? 0u : body->filter.layer;
}

std::optional<glm::mat4> PhysicsWorld::bodyWorldMatrix(PhysicsBodyId id) const
{
    const Body *body = find(id);
    if (body == nullptr)
    {
        return std::nullopt;
    }
    return ShapeCollision::composeTransform(body->position, body->rotation);
}

std::optional<Aabb> PhysicsWorld::bodyWorldAabb(PhysicsBodyId id) const
{
    const Body *body = find(id);
    if (body == nullptr)
    {
        return std::nullopt;
    }
    return body->worldAabb;
}

std::optional<RigidBodyState> PhysicsWorld::bodyState(PhysicsBodyId id) const
{
    const Body *body = find(id);
    if (body == nullptr || !body->dynamic)
    {
        return std::nullopt;
    }
    RigidBodyState state;
    state.position = body->position;
    state.rotation = body->rotation;
    state.velocity = body->velocity;
    state.angularVelocity = body->angularVelocity;
    state.sleeping = body->sleeping;
    return state;
}

std::optional<RigidBodyState> PhysicsWorld::interpolatedBodyState(PhysicsBodyId id, float alpha) const
{
    const std::optional<RigidBodyState> current = bodyState(id);
    const Body *body = find(id);
    if (!current || body == nullptr)
    {
        return std::nullopt;
    }
    if (!std::isfinite(alpha))
    {
        throw std::invalid_argument("Interpolation alpha must be finite");
    }
    const float clamped = std::clamp(alpha, 0.0f, 1.0f);
    RigidBodyState state = *current;
    state.position = glm::mix(body->previousPosition, body->position, clamped);
    // 静止或休眠的直接取当前值，避免插值出多余旋转。
    state.rotation = (body->sleeping || body->previousRotation == body->rotation)
        ? body->rotation
        : glm::normalize(glm::slerp(body->previousRotation, body->rotation, clamped));
    return state;
}

bool PhysicsWorld::setBodyVelocity(PhysicsBodyId id, const glm::vec3 &velocity)
{
    Body *body = find(id);
    if (body == nullptr || !body->dynamic)
    {
        return false;
    }
    if (!isFinite(velocity))
    {
        throw std::invalid_argument("Body velocity must be finite");
    }
    body->velocity = velocity;
    body->sleeping = false;
    body->sleepTimer = 0.0f;
    return true;
}

bool PhysicsWorld::setBodyAngularVelocity(PhysicsBodyId id, const glm::vec3 &angularVelocity)
{
    Body *body = find(id);
    if (body == nullptr || !body->dynamic)
    {
        return false;
    }
    if (!isFinite(angularVelocity))
    {
        throw std::invalid_argument("Body angular velocity must be finite");
    }
    body->angularVelocity = angularVelocity;
    body->sleeping = false;
    body->sleepTimer = 0.0f;
    return true;
}

bool PhysicsWorld::applyImpulse(PhysicsBodyId id, const glm::vec3 &impulse)
{
    Body *body = find(id);
    if (body == nullptr || !body->dynamic)
    {
        return false;
    }
    if (!isFinite(impulse))
    {
        throw std::invalid_argument("Impulse must be finite");
    }
    body->velocity += impulse / body->settings.mass;
    body->sleeping = false;
    body->sleepTimer = 0.0f;
    return true;
}

bool PhysicsWorld::applyForce(PhysicsBodyId id, const glm::vec3 &force)
{
    Body *body = find(id);
    if (body == nullptr || !body->dynamic)
    {
        return false;
    }
    if (!isFinite(force))
    {
        throw std::invalid_argument("Force must be finite");
    }
    body->force += force;
    body->sleeping = false;
    body->sleepTimer = 0.0f;
    return true;
}

bool PhysicsWorld::clearForces(PhysicsBodyId id)
{
    Body *body = find(id);
    if (body == nullptr || !body->dynamic)
    {
        return false;
    }
    body->force = glm::vec3(0.0f);
    return true;
}

bool PhysicsWorld::wakeBody(PhysicsBodyId id)
{
    Body *body = find(id);
    if (body == nullptr || !body->dynamic)
    {
        return false;
    }
    body->sleeping = false;
    body->sleepTimer = 0.0f;
    return true;
}

bool PhysicsWorld::isSleeping(PhysicsBodyId id) const
{
    const Body *body = find(id);
    return body != nullptr && body->dynamic && body->sleeping;
}

std::optional<PhysicsWorld::RaycastResult> PhysicsWorld::raycast(const Ray &ray, float maxDistance,
    std::uint32_t queryMask) const
{
    return PhysicsQueries::raycast(*this, ray, maxDistance, queryMask);
}

std::vector<PhysicsWorld::OverlapResult> PhysicsWorld::overlapShape(const CollisionShape &shape,
    const glm::vec3 &position, const glm::quat &rotation, std::uint32_t queryMask) const
{
    return PhysicsQueries::overlapShape(*this, shape, position, rotation, queryMask);
}

std::vector<std::pair<PhysicsBodyId, PhysicsBodyId>> PhysicsWorld::broadphasePairs() const
{
    return PhysicsQueries::broadphasePairs(*this);
}

void PhysicsWorld::refreshWorldAabb(Body &body)
{
    // 先构造位姿：非法位置或零四元数会在这里抛出，因此每次位姿变更都会得到校验。
    const glm::mat4 worldMatrix = ShapeCollision::composeTransform(body.position, body.rotation);
    if (!body.shape.isFinite())
    {
        body.worldAabb.reset(); // 平面没有有限包围盒。
        return;
    }
    body.worldAabb = body.shape.localAabb().transformed(worldMatrix);
}

PhysicsWorld::Body *PhysicsWorld::find(PhysicsBodyId id)
{
    const auto entry = indexById_.find(id);
    return entry == indexById_.end() ? nullptr : bodies_[entry->second].get();
}

const PhysicsWorld::Body *PhysicsWorld::find(PhysicsBodyId id) const
{
    const auto entry = indexById_.find(id);
    return entry == indexById_.end() ? nullptr : bodies_[entry->second].get();
}
