#pragma once

#include "physics/body/PhysicsFilter.h"
#include "physics/shapes/CollisionShape.h"
#include "physics/world/PhysicsWorld.h"

#include <cstdint>
#include <optional>

class Transform;

// PhysicsBodyComponent是GameObject的物理组件，规则与Renderable一致：只描述"这个物体
// 以什么形状参与碰撞"，不负责物体身份、Transform或更新逻辑。
//
// 借用模式：组件借用PhysicsWorld和GameObject的Transform，不拥有它们。世界必须比
// 所有引用它的物体活得更久，否则注册关系会在世界销毁后悬空。
// 首版只注册静态体（位置由应用在需要时通过syncFromTransform同步）；动力学体在P3加入。
class PhysicsBodyComponent
{
public:
    PhysicsBodyComponent() = default;
    // 注销仍在注册中的静态体，避免世界里残留指向已销毁物体的句柄。
    ~PhysicsBodyComponent();
    PhysicsBodyComponent(const PhysicsBodyComponent &) = delete;
    PhysicsBodyComponent &operator=(const PhysicsBodyComponent &) = delete;
    PhysicsBodyComponent(PhysicsBodyComponent &&) = delete;
    PhysicsBodyComponent &operator=(PhysicsBodyComponent &&) = delete;

    // 注册为静态体；形状按值保存，避免借用调用方的临时形状。
    // 已经注册时先注销旧体再注册新体；world为空或形状非法时抛std::invalid_argument。
    void attach(PhysicsWorld &world, const CollisionShape &shape);
    // 用形状与碰撞过滤注册在给定位姿上；物体需要在注册时就位于非原点位置时使用。
    void attach(PhysicsWorld &world, const CollisionShape &shape, const glm::vec3 &position,
        const glm::quat &rotation, const PhysicsFilter &filter = {});
    // 注销静态体并清除形状；未注册时无副作用。
    void detach();

    bool isAttached() const noexcept;
    // 未注册时返回0；句柄在世界重建后不再有效。
    PhysicsBodyId bodyId() const noexcept;
    // 返回组件保存的形状；未注册时返回nullptr。
    const CollisionShape *shape() const noexcept;

    // 把Transform的位姿同步到静态体。静态体移动后必须调用，否则查询结果与渲染不一致。
    // 未注册返回false；非法位姿由PhysicsWorld抛出。
    bool syncFromTransform(const Transform &transform);

private:
    // 借用，不负责销毁；组件析构时用它注销静态体。
    PhysicsWorld *world_ = nullptr;
    std::optional<CollisionShape> shape_;
    PhysicsBodyId bodyId_ = 0;
};
