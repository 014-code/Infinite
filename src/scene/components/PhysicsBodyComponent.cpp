#include "scene/components/PhysicsBodyComponent.h"

#include "math/Transform.h"

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
    detach();
    const PhysicsBodyId id = world.createStaticBody(shape, position, rotation, filter);
    world_ = &world;
    shape_ = shape;
    bodyId_ = id;
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
}

bool PhysicsBodyComponent::isAttached() const noexcept
{
    return bodyId_ != 0;
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
    if (world_ == nullptr || bodyId_ == 0)
    {
        return false;
    }
    return world_->setBodyTransform(bodyId_, transform.position, transform.rotation());
}
