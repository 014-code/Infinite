#include "scene/components/PhysicsBodyComponent.h"

#include "math/Transform.h"
#include "physics/world/PhysicsWorld.h"
#include "scene/components/PhysicsTransformRules.h"

#include <stdexcept>

PhysicsBodyComponent::~PhysicsBodyComponent()
{
    detach();
}

void PhysicsBodyComponent::attach(PhysicsWorld &world, const CollisionShape &shape)
{
    attach(world, shape, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
}

void PhysicsBodyComponent::attach(PhysicsWorld &world, const CollisionShape &shape, const glm::vec3 &position,
    const glm::quat &rotation, const PhysicsFilter &filter)
{
    if (shape.holds<PlaneShape>())
    {
        // 平面没有有限包围盒，作为物体碰撞形状时缺少可同步的位姿语义；
        // 静态世界几何请直接注册到PhysicsWorld。
        throw std::invalid_argument("PhysicsBodyComponent does not accept a PlaneShape");
    }

    // 所有可能失败的输入检查都放在替换旧注册之前。这样调用方尝试绑定
    // 一个非法位姿时，旧的碰撞体仍然保持可用。
    const glm::quat normalizedRotation = ShapeCollision::normalizedRotation(rotation);
    ShapeCollision::composeTransform(position, normalizedRotation);
    std::optional<CollisionShape> replacementShape = shape;
    const PhysicsBodyId id = world.createStaticBody(*replacementShape, position, normalizedRotation, filter);

    // 新体已经成功创建，下面只交换值和句柄，不再执行会抛异常的校验。
    // 旧世界可能与新世界不同，所以先保存旧注册，再在提交后注销它。
    PhysicsWorld *oldWorld = world_;
    const PhysicsBodyId oldBodyId = bodyId_;
    shape_.swap(replacementShape);
    world_ = &world;
    bodyId_ = id;
    type_ = PhysicsBodyType::Static;
    if (oldWorld != nullptr && oldBodyId != 0)
    {
        oldWorld->destroyBody(oldBodyId);
    }
}

void PhysicsBodyComponent::attachDynamic(PhysicsWorld &world, const CollisionShape &shape,
    const glm::vec3 &position, const glm::quat &rotation, const RigidBodySettings &settings,
    const PhysicsFilter &filter)
{
    const glm::quat normalizedRotation = ShapeCollision::normalizedRotation(rotation);
    ShapeCollision::composeTransform(position, normalizedRotation);
    std::optional<CollisionShape> replacementShape = shape;
    PhysicsBodyId id = 0;
    try
    {
        // 先在旧体仍然存在时创建新体；参数错误不会破坏旧绑定。
        id = world.createDynamicBody(*replacementShape, position, settings, filter);
        // PhysicsWorld目前的动态体创建接口使用单位旋转，因此在创建后补上初始旋转。
        // 如果这一步失败，catch会撤销已经注册的Body，避免世界留下组件不知道的孤儿体。
        if (normalizedRotation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
        {
            world.setBodyTransform(id, position, normalizedRotation);
        }
    }
    catch (...)
    {
        if (id != 0)
        {
            world.destroyBody(id);
        }
        throw;
    }

    PhysicsWorld *oldWorld = world_;
    const PhysicsBodyId oldBodyId = bodyId_;
    shape_.swap(replacementShape);
    world_ = &world;
    bodyId_ = id;
    type_ = PhysicsBodyType::Dynamic;
    if (oldWorld != nullptr && oldBodyId != 0)
    {
        oldWorld->destroyBody(oldBodyId);
    }
}

void PhysicsBodyComponent::detach()
{
    if (world_ != nullptr && bodyId_ != 0)
    {
        world_->destroyBody(bodyId_);
    }
    world_ = nullptr;
    shape_.reset();
    bodyId_ = 0;
    type_ = PhysicsBodyType::Static;
}

bool PhysicsBodyComponent::isAttached() const noexcept
{
    return world_ != nullptr && bodyId_ != 0 && world_->contains(bodyId_);
}

bool PhysicsBodyComponent::isDynamic() const noexcept
{
    return isAttached() && type_ == PhysicsBodyType::Dynamic;
}

PhysicsBodyType PhysicsBodyComponent::type() const noexcept
{
    return type_;
}

bool PhysicsBodyComponent::belongsTo(const PhysicsWorld &world) const noexcept
{
    return world_ == &world && isAttached();
}

PhysicsBodyId PhysicsBodyComponent::bodyId() const noexcept
{
    return bodyId_;
}

const CollisionShape *PhysicsBodyComponent::shape() const noexcept
{
    return shape_ ? &*shape_ : nullptr;
}

bool PhysicsBodyComponent::syncFromTransform(const Transform &transform)
{
    if (!isAttached() || type_ != PhysicsBodyType::Static)
    {
        return false;
    }
    if (transform.parent() != nullptr)
    {
        throw std::invalid_argument("Physics body Transform must be a root Transform");
    }
    PhysicsTransformRules::validate(transform);
    return world_->setBodyTransform(bodyId_, transform.position, transform.rotation());
}

bool PhysicsBodyComponent::syncToTransform(Transform &transform, float interpolationAlpha) const
{
    if (!isAttached() || type_ != PhysicsBodyType::Dynamic)
    {
        return false;
    }
    if (transform.parent() != nullptr)
    {
        throw std::invalid_argument("Physics body Transform must be a root Transform");
    }
    PhysicsTransformRules::validate(transform);
    const std::optional<RigidBodyState> state = world_->interpolatedBodyState(bodyId_, interpolationAlpha);
    if (!state)
    {
        return false;
    }
    transform.position = state->position;
    transform.setRotation(state->rotation);
    return true;
}
