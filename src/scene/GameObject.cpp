#include "GameObject.h"

#include "physics/world/PhysicsWorld.h"
#include "scene/components/PhysicsTransformRules.h"

#include <utility>

GameObject::GameObject(ObjectId id, const std::string &name)
    : id_(id), name_(name), script_(*this), audioSource_(*this), renderable_(*this),
      physicsBody_(*this), characterBody_(*this), area_(*this)
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
    script_.setUpdateCallback(std::move(callback));
}

ScriptComponent &GameObject::script()
{
    return script_;
}

const ScriptComponent &GameObject::script() const
{
    return script_;
}

void GameObject::setAudioSource(AudioSystem &audio, std::shared_ptr<const AudioClip> clip)
{
    audioSource_.attach(audio, std::move(clip));
}

void GameObject::clearAudioSource() noexcept
{
    audioSource_.detach();
}

AudioSourceComponent &GameObject::audioSource()
{
    return audioSource_;
}

const AudioSourceComponent &GameObject::audioSource() const
{
    return audioSource_;
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
    PhysicsTransformRules::validate(transform);
    // 物体自己的位姿作为注册位姿，避免注册后还要额外同步一次。
    physicsBody_.attach(world, shape, transform.position, transform.rotation(), filter);
    // 先让新组件完成注册，再解除旧角色组件；如果注册失败，旧控制器仍保持不变。
    characterBody_.detach();
}

void GameObject::setDynamicPhysicsBody(PhysicsWorld &world, const CollisionShape &shape,
    const RigidBodySettings &settings, const PhysicsFilter &filter)
{
    PhysicsTransformRules::validate(transform);
    // 动态体同样使用物体当前Transform作为初始位置，之后由Scene在物理步后自动同步。
    physicsBody_.attachDynamic(world, shape, transform.position, transform.rotation(), settings, filter);
    characterBody_.detach();
}

void GameObject::setCharacterBody(PhysicsWorld &world, const CharacterSettings &settings,
    std::uint32_t queryMask)
{
    PhysicsTransformRules::validate(transform);
    characterBody_.attach(world, settings, transform.position, queryMask);
    // 新角色绑定成功后才移除旧刚体，避免非法角色参数破坏已有物理组件。
    physicsBody_.detach();
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

CharacterBodyComponent &GameObject::characterBody()
{
    return characterBody_;
}

const CharacterBodyComponent &GameObject::characterBody() const
{
    return characterBody_;
}

void GameObject::setArea(PhysicsWorld &world, const CollisionShape &shape,
    std::uint32_t queryMask)
{
    // Area和PhysicsBody一样把物体Transform解释为世界空间位姿；
    // 先校验再替换旧Area，失败时不会破坏原有查询区域。
    PhysicsTransformRules::validate(transform);
    area_.attach(world, shape, queryMask);
}

void GameObject::clearArea() noexcept
{
    area_.detach();
}

AreaComponent &GameObject::area()
{
    return area_;
}

const AreaComponent &GameObject::area() const
{
    return area_;
}
