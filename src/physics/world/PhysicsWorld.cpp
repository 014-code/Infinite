#include "physics/world/PhysicsWorld.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    constexpr float POSITION_SLOP = 0.001f;
    // 位置修正比例：低于1可以避免多个接触互相抢修造成抖动，代价是每步残留少量穿透。
    constexpr float POSITION_CORRECTION = 0.8f;
    // 恢复系数速度阈值：接近速度低于该值时不弹跳，否则靠重力压出的微小穿透会让
    // 静止物体永远以微小速度抖动，无法进入休眠。
    constexpr float RESTITUTION_VELOCITY_THRESHOLD = 1.0f;
    constexpr float SLEEP_ANGULAR_THRESHOLD = 0.2f;

    bool isFinite(const glm::vec3 &value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    // 射线与包围盒的快速排除；不相交的体不进入精确求交。
    bool rayHitsAabb(const Ray &ray, const Aabb &box)
    {
        return PhysicsRaycast::intersectBox(ray, box).has_value();
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
        stepFixedOnce(fixedStep_);
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
    indexById_[id] = bodies_.size() - 1;
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
    bodies_.erase(bodies_.begin() + static_cast<std::ptrdiff_t>(entry->second));
    // 删除会移动后续元素的下标，映射必须重建；这一步是O(n)，但删除本身远少于查询。
    rebuildIndex();
    return true;
}

void PhysicsWorld::clear()
{
    bodies_.clear();
    indexById_.clear();
    accumulator_ = 0.0f;
    lastNarrowphasePairCount_ = 0;
}

void PhysicsWorld::rebuildIndex()
{
    indexById_.clear();
    indexById_.reserve(bodies_.size());
    for (std::size_t index = 0; index < bodies_.size(); ++index)
    {
        indexById_[bodies_[index]->id] = index;
    }
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
    ids.reserve(bodies_.size());
    for (const std::unique_ptr<Body> &body : bodies_)
    {
        ids.push_back(body->id);
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
    if (!std::isfinite(maxDistance) || maxDistance < 0.0f)
    {
        throw std::invalid_argument("Raycast maxDistance must be finite and non-negative");
    }
    std::optional<RaycastResult> best;
    for (const std::unique_ptr<Body> &body : bodies_)
    {
        if (!filterMatchesQuery(body->filter, queryMask))
        {
            continue;
        }
        // 有限形状先用包围盒排除；平面没有包围盒，直接进入精确求交。
        if (body->worldAabb && !rayHitsAabb(ray, *body->worldAabb))
        {
            continue;
        }

        std::optional<RaycastHit> hit;
        if (body->shape.holds<PlaneShape>())
        {
            glm::vec3 normal(0.0f);
            float offset = 0.0f;
            ShapeCollision::planeToWorld(body->shape.get<PlaneShape>(), body->position, body->rotation, normal,
                offset);
            hit = PhysicsRaycast::intersectPlane(ray, normal, offset);
        }
        else
        {
            // 碰撞体不含缩放，因此把射线变换到局部空间后t与世界空间一致。
            const glm::mat4 inverseWorld = glm::inverse(ShapeCollision::composeTransform(body->position,
                body->rotation));
            const glm::vec3 localOrigin(inverseWorld * glm::vec4(ray.origin, 1.0f));
            const glm::vec3 localDirection(inverseWorld * glm::vec4(ray.direction, 0.0f));
            const Ray localRay(localOrigin, localDirection);

            if (body->shape.holds<SphereShape>())
            {
                hit = PhysicsRaycast::intersectSphere(localRay, glm::vec3(0.0f),
                    body->shape.get<SphereShape>().radius);
            }
            else if (body->shape.holds<BoxShape>())
            {
                const glm::vec3 &halfExtents = body->shape.get<BoxShape>().halfExtents;
                hit = PhysicsRaycast::intersectBox(localRay, Aabb(-halfExtents, halfExtents));
            }
            else if (body->shape.holds<CapsuleShape>())
            {
                const CapsuleShape &capsule = body->shape.get<CapsuleShape>();
                hit = PhysicsRaycast::intersectCapsule(localRay, capsule.radius, capsule.cylinderHeight);
            }

            // 命中点与法线需要回到世界空间；t在刚体变换下不变。
            if (hit)
            {
                const glm::mat4 worldMatrix = ShapeCollision::composeTransform(body->position, body->rotation);
                hit->point = glm::vec3(worldMatrix * glm::vec4(hit->point, 1.0f));
                hit->normal = body->rotation * hit->normal;
            }
        }

        if (hit && hit->t <= maxDistance && (!best || hit->t < best->hit.t))
        {
            best = RaycastResult{body->id, *hit, body->dynamic};
        }
    }
    return best;
}

std::vector<PhysicsWorld::OverlapResult> PhysicsWorld::overlapShape(const CollisionShape &shape,
    const glm::vec3 &position, const glm::quat &rotation, std::uint32_t queryMask) const
{
    // 查询形状是平面时没有有限范围，且平面-平面组合不受支持，直接报错而不是返回空结果。
    if (shape.holds<PlaneShape>())
    {
        throw std::invalid_argument("overlapShape does not accept a PlaneShape query");
    }
    const Aabb queryAabb = shape.localAabb().transformed(ShapeCollision::composeTransform(position, rotation));

    std::vector<OverlapResult> results;
    for (const std::unique_ptr<Body> &body : bodies_)
    {
        if (!filterMatchesQuery(body->filter, queryMask))
        {
            continue;
        }
        if (body->worldAabb && !queryAabb.intersects(*body->worldAabb))
        {
            continue;
        }
        const std::optional<ShapeContact> contact = ShapeCollision::collide(shape, position, rotation,
            body->shape, body->position, body->rotation);
        if (contact)
        {
            results.push_back(OverlapResult{body->id, *contact,
                ShapeCollision::separationDirection(*contact, body->shape.holds<PlaneShape>()), body->dynamic});
        }
    }
    return results;
}

void PhysicsWorld::collectSweepCandidates(std::vector<std::pair<std::size_t, std::size_t>> &candidates) const
{
    // 平面没有有限包围盒，不参与扫掠剪枝；它们由collectContacts单独与动态体配对。
    std::vector<std::size_t> sorted;
    sorted.reserve(bodies_.size());
    for (std::size_t index = 0; index < bodies_.size(); ++index)
    {
        if (bodies_[index]->worldAabb)
        {
            sorted.push_back(index);
        }
    }
    std::sort(sorted.begin(), sorted.end(),
        [this](std::size_t left, std::size_t right)
        {
            return bodies_[left]->worldAabb->min.x < bodies_[right]->worldAabb->min.x;
        });

    for (std::size_t i = 0; i < sorted.size(); ++i)
    {
        const Aabb &current = *bodies_[sorted[i]]->worldAabb;
        for (std::size_t j = i + 1; j < sorted.size(); ++j)
        {
            const Body &candidate = *bodies_[sorted[j]];
            if (candidate.worldAabb->min.x > current.max.x)
            {
                break; // 后续体的min.x只会更大，可以结束本轮的扫描。
            }
            if (!current.intersects(*candidate.worldAabb))
            {
                continue;
            }
            if (!filtersInteract(bodies_[sorted[i]]->filter, candidate.filter))
            {
                continue; // 层/掩码不允许这一对，连候选对也不产生。
            }
            candidates.emplace_back(sorted[i], sorted[j]);
        }
    }
}

std::vector<std::pair<PhysicsBodyId, PhysicsBodyId>> PhysicsWorld::broadphasePairs() const
{
    std::vector<std::pair<std::size_t, std::size_t>> candidates;
    collectSweepCandidates(candidates);

    std::vector<std::pair<PhysicsBodyId, PhysicsBodyId>> pairs;
    pairs.reserve(candidates.size());
    for (const std::pair<std::size_t, std::size_t> &candidate : candidates)
    {
        pairs.emplace_back(bodies_[candidate.first]->id, bodies_[candidate.second]->id);
    }
    return pairs;
}

void PhysicsWorld::stepFixedOnce(float stepSeconds)
{
    // 1. 积分：只有未休眠的动态体参与。
    for (std::unique_ptr<Body> &body : bodies_)
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
        body->velocity += (gravity_ + body->force / body->settings.mass) * stepSeconds;
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
    collectContacts(contacts);
    if (!contacts.empty())
    {
        solveVelocity(contacts);
        correctPositions(contacts);
    }

    // 3. 休眠判定与包围盒刷新。
    for (std::unique_ptr<Body> &body : bodies_)
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
        refreshWorldAabb(*body);
    }
    ++fixedStepCount_;
}

void PhysicsWorld::collectContacts(std::vector<Contact> &contacts) const
{
    // 只有至少一侧是"醒着的动态体"时，接触才可能改变状态：
    // 静态-静态、休眠动态-静态、休眠-休眠都不必进入窄相位。
    const auto awakeDynamic = [this](std::size_t index)
    {
        const Body &body = *bodies_[index];
        return body.dynamic && !body.sleeping;
    };

    // 有限体之间用扫掠剪枝候选对，避免对所有body做O(n²)全配对。
    std::vector<std::pair<std::size_t, std::size_t>> candidates;
    collectSweepCandidates(candidates);

    std::size_t pairCount = 0;
    for (const std::pair<std::size_t, std::size_t> &candidate : candidates)
    {
        const std::size_t first = candidate.first;
        const std::size_t second = candidate.second;
        if (!awakeDynamic(first) && !awakeDynamic(second))
        {
            continue;
        }
        // buildContact约定第一个下标是动态体，法线方向才是"把动态体推开"。
        if (bodies_[first]->dynamic)
        {
            buildContact(first, second, contacts);
        }
        else
        {
            buildContact(second, first, contacts);
        }
        ++pairCount;
    }

    // 平面没有有限包围盒，无法参与扫掠；平面数量很少，直接与每个醒着的动态体配对。
    for (std::size_t planeIndex = 0; planeIndex < bodies_.size(); ++planeIndex)
    {
        if (!bodies_[planeIndex]->shape.holds<PlaneShape>())
        {
            continue;
        }
        for (std::size_t dynamicIndex = 0; dynamicIndex < bodies_.size(); ++dynamicIndex)
        {
            if (dynamicIndex == planeIndex || !awakeDynamic(dynamicIndex))
            {
                continue;
            }
            if (!filtersInteract(bodies_[dynamicIndex]->filter, bodies_[planeIndex]->filter))
            {
                continue;
            }
            buildContact(dynamicIndex, planeIndex, contacts);
            ++pairCount;
        }
    }
    lastNarrowphasePairCount_ = pairCount;
}

void PhysicsWorld::buildContact(std::size_t indexA, std::size_t indexB, std::vector<Contact> &contacts) const
{
    const Body &a = *bodies_[indexA];
    const Body &b = *bodies_[indexB];
    const std::optional<ShapeContact> contact = ShapeCollision::collide(a.shape, a.position, a.rotation,
        b.shape, b.position, b.rotation);
    if (!contact)
    {
        return;
    }

    // ShapeCollision的法线约定：平面参与时是平面正侧法线，其余是"A指向B"。
    // separationDirection把两种约定统一成"把动态体沿+normal推开"的分离方向。
    const glm::vec3 separation = ShapeCollision::separationDirection(*contact, b.shape.holds<PlaneShape>());

    Contact result;
    result.a = indexA;
    result.b = b.dynamic ? indexB : NO_BODY;
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

void PhysicsWorld::solveVelocity(std::vector<Contact> &contacts)
{
    for (int iteration = 0; iteration < solverIterations_; ++iteration)
    {
        for (Contact &contact : contacts)
        {
            Body &a = *bodies_[contact.a];
            Body &b = contact.b == NO_BODY ? a : *bodies_[contact.b];
            const bool hasB = contact.b != NO_BODY;

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

void PhysicsWorld::correctPositions(std::vector<Contact> &contacts)
{
    // 位置修正独立于速度求解：直接按穿透深度把动态体推开，带slop避免抖动。
    for (const Contact &contact : contacts)
    {
        Body &a = *bodies_[contact.a];
        const bool hasB = contact.b != NO_BODY;
        Body *b = hasB ? bodies_[contact.b].get() : nullptr;
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
