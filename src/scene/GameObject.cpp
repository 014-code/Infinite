#include "GameObject.h"

#include <utility>

GameObject::GameObject(ObjectId id, const std::string &name)
    : id_(id), name_(name)
{
}

ObjectId GameObject::id() const
{
    return id_;
}

const std::string &GameObject::name() const
{
    return name_;
}

void GameObject::setName(const std::string &name)
{
    name_ = name;
}

bool GameObject::isActive() const
{
    return active_;
}

void GameObject::setActive(bool active)
{
    active_ = active;
}

void GameObject::setUpdateCallback(UpdateCallback callback)
{
    updateCallback_ = callback ? std::make_shared<UpdateCallback>(std::move(callback)) : nullptr;
}

void GameObject::setRenderable(const Mesh &mesh, const Material &material)
{
    renderable_.set(mesh, material);
}

void GameObject::setRenderable(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material> material)
{
    renderable_.set(std::move(mesh), std::move(material));
}

void GameObject::clearRenderable()
{
    renderable_.clear();
}

Renderable &GameObject::renderable()
{
    return renderable_;
}

const Renderable &GameObject::renderable() const
{
    return renderable_;
}

const Mesh *GameObject::mesh() const
{
    return renderable_.mesh();
}

const Material *GameObject::material() const
{
    return renderable_.material();
}

const glm::vec3 &GameObject::sortOrigin() const
{
    return renderable_.sortOrigin();
}

void GameObject::setSortOrigin(const glm::vec3 &origin)
{
    renderable_.setSortOrigin(origin);
}

void GameObject::setPhysicsBody(PhysicsWorld &world, const CollisionShape &shape, const PhysicsFilter &filter)
{
    // 物体自己的位姿作为注册位姿，避免注册后还要额外同步一次。
    physicsBody_.attach(world, shape, transform.position, transform.rotation(), filter);
}

void GameObject::clearPhysicsBody()
{
    physicsBody_.detach();
}

PhysicsBodyComponent &GameObject::physicsBody()
{
    return physicsBody_;
}

const PhysicsBodyComponent &GameObject::physicsBody() const
{
    return physicsBody_;
}
