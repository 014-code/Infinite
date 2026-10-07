#pragma once

#include "physics/body/PhysicsBodyId.h"
#include "physics/shapes/CollisionShape.h"
#include "scene/components/Component.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>
#include <utility>

class PhysicsWorld;
class Transform;

// AreaComponent是一个不参与碰撞响应的查询区域，也就是常说的Trigger/Area。
// 它使用PhysicsWorld的overlapShape查询，并通过前后两次集合差生成进入/离开事件。
class AreaComponent final : public Component
{
public:
    using BodyCallback = std::function<void(PhysicsBodyId)>;

    AreaComponent() = default;
    explicit AreaComponent(GameObject &owner) noexcept : Component(owner) {}
    ~AreaComponent() override = default;

    AreaComponent(const AreaComponent &) = delete;
    AreaComponent &operator=(const AreaComponent &) = delete;
    AreaComponent(AreaComponent &&) = delete;
    AreaComponent &operator=(AreaComponent &&) = delete;

    // 区域本身不注册为PhysicsBody，因此不会挡住或推动其他物体。
    // shape按值保存；queryMask只决定能检测哪些碰撞层。
    void attach(PhysicsWorld &world, const CollisionShape &shape,
        std::uint32_t queryMask = 0xFFFFFFFFu);
    void detach() noexcept;

    bool isAttached() const noexcept { return world_ != nullptr && shape_.has_value(); }
    bool belongsTo(const PhysicsWorld &world) const noexcept { return world_ == &world; }
    const CollisionShape *shape() const noexcept { return shape_ ? &*shape_ : nullptr; }
    std::uint32_t queryMask() const noexcept { return queryMask_; }
    void setQueryMask(std::uint32_t mask) noexcept { queryMask_ = mask; }

    void setBodyEnteredCallback(BodyCallback callback) { bodyEntered_ = std::move(callback); }
    void setBodyExitedCallback(BodyCallback callback) { bodyExited_ = std::move(callback); }

    // Scene在固定物理步完成后调用；Transform使用物体当前的位置和旋转。
    // 先更新集合，再调用用户回调，避免回调查询时看到半更新状态。
    void poll(const Transform &transform);
    void clearOverlaps() noexcept { overlapping_.clear(); }

private:
    PhysicsWorld *world_ = nullptr;
    std::optional<CollisionShape> shape_;
    std::uint32_t queryMask_ = 0xFFFFFFFFu;
    std::unordered_set<PhysicsBodyId> overlapping_;
    BodyCallback bodyEntered_;
    BodyCallback bodyExited_;
};
