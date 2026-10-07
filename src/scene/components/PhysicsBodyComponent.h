#pragma once

#include "physics/body/PhysicsBodyId.h"
#include "physics/body/PhysicsFilter.h"
#include "physics/body/RigidBody.h"
#include "physics/shapes/CollisionShape.h"
#include "scene/components/Component.h"

#include <cstdint>
#include <optional>

class Transform;
class PhysicsWorld;

enum class PhysicsBodyType
{
    Static,
    Dynamic
};

// PhysicsBodyComponent是GameObject的物理组件，规则与Renderable一致：只描述"这个物体
// 以什么形状参与碰撞"，不负责物体身份、Transform或更新逻辑。
//
// 借用模式：组件借用PhysicsWorld和GameObject的Transform，不拥有它们。世界必须比
// 所有引用它的物体活得更久，否则注册关系会在世界销毁后悬空。
// 静态体由Transform驱动，动态体由PhysicsWorld驱动。
// 两种模式共用生命周期接口，但不会在同一帧互相覆盖Transform。
class PhysicsBodyComponent : public Component
{
public:
    PhysicsBodyComponent() = default;
    explicit PhysicsBodyComponent(GameObject &owner) noexcept : Component(owner) {}
    // 注销仍在注册中的静态体，避免世界里残留指向已销毁物体的句柄。
    ~PhysicsBodyComponent() override;
    PhysicsBodyComponent(const PhysicsBodyComponent &) = delete;
    PhysicsBodyComponent &operator=(const PhysicsBodyComponent &) = delete;
    PhysicsBodyComponent(PhysicsBodyComponent &&) = delete;
    PhysicsBodyComponent &operator=(PhysicsBodyComponent &&) = delete;

    // 注册为静态体；形状按值保存，避免借用调用方的临时形状。
    // 已经注册时，只有新体创建成功才会替换旧体；world为空或形状非法时抛std::invalid_argument。
    void attach(PhysicsWorld &world, const CollisionShape &shape);
    // 用形状与碰撞过滤注册在给定位姿上；物体需要在注册时就位于非原点位置时使用。
    void attach(PhysicsWorld &world, const CollisionShape &shape, const glm::vec3 &position,
        const glm::quat &rotation, const PhysicsFilter &filter = {});
    // 注册为动态刚体。动态体的位姿由PhysicsWorld积分，初始位置取调用方传入值。
    // 当前PhysicsWorld仍只接受SphereShape作为动态体，非法组合会明确抛出异常。
    void attachDynamic(PhysicsWorld &world, const CollisionShape &shape, const glm::vec3 &position,
        const glm::quat &rotation, const RigidBodySettings &settings = {},
        const PhysicsFilter &filter = {});
    // 注销当前体并清除形状；未注册时无副作用。
    void detach();

    bool isAttached() const noexcept;
    // 未注册时返回false；动态体与静态体都可以通过此接口区分。
    bool isDynamic() const noexcept;
    PhysicsBodyType type() const noexcept;
    // Scene同步时用它过滤掉挂在其他PhysicsWorld上的组件。
    bool belongsTo(const PhysicsWorld &world) const noexcept;
    // 未注册时返回0；句柄在世界重建后不再有效。
    PhysicsBodyId bodyId() const noexcept;
    // 返回组件保存的形状；未注册时返回nullptr。
    const CollisionShape *shape() const noexcept;

    // 把Transform的位姿同步到静态体。静态体移动后必须调用，否则查询结果与渲染不一致。
    // 未注册返回false；非法位姿由PhysicsWorld抛出。
    bool syncFromTransform(const Transform &transform);
    // 把动态体的当前物理位姿写回Transform。渲染时使用插值状态，减少固定步长抖动。
    // 静态体、未注册体或已经被世界清除的体返回false。
    bool syncToTransform(Transform &transform, float interpolationAlpha) const;

private:
    // 借用，不负责销毁；组件析构时用它注销静态体。
    PhysicsWorld *world_ = nullptr;
    std::optional<CollisionShape> shape_;
    PhysicsBodyId bodyId_ = 0;
    PhysicsBodyType type_ = PhysicsBodyType::Static;
};
