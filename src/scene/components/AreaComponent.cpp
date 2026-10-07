#include "AreaComponent.h"

#include "math/Transform.h"
#include "physics/world/PhysicsWorld.h"
#include "scene/GameObject.h"

#include <algorithm>
#include <utility>
#include <vector>
#include <stdexcept>

void AreaComponent::attach(PhysicsWorld &world, const CollisionShape &shape,
    std::uint32_t queryMask)
{
    // 先验证并复制新配置，再替换旧区域；失败时旧区域仍然有效。
    std::optional<CollisionShape> newShape(shape);
    if (newShape->holds<PlaneShape>())
    {
        throw std::invalid_argument("Area shape must have finite bounds");
    }
    world_ = &world;
    shape_ = std::move(newShape);
    queryMask_ = queryMask;
    overlapping_.clear();
}

void AreaComponent::detach() noexcept
{
    world_ = nullptr;
    shape_.reset();
    overlapping_.clear();
}

void AreaComponent::poll(const Transform &transform)
{
    if (!isAttached())
    {
        return;
    }

    const auto overlaps = world_->overlapShape(*shape_, transform.position,
        transform.rotation(), queryMask_);
    std::unordered_set<PhysicsBodyId> current;
    current.reserve(overlaps.size());
    for (const auto &overlap : overlaps)
    {
        // 如果同一物体同时挂有碰撞体和Area，不能把自己的碰撞体报告成进入事件。
        if (owner() != nullptr && owner()->physicsBody().bodyId() == overlap.body)
        {
            continue;
        }
        current.insert(overlap.body);
    }

    std::vector<PhysicsBodyId> entered;
    std::vector<PhysicsBodyId> exited;
    for (PhysicsBodyId id : current)
    {
        if (overlapping_.find(id) == overlapping_.end()) { entered.push_back(id); }
    }
    for (PhysicsBodyId id : overlapping_)
    {
        if (current.find(id) == current.end()) { exited.push_back(id); }
    }
    // unordered_set只负责高效去重；回调顺序固定后，录制、测试和游戏规则更容易复现。
    std::sort(entered.begin(), entered.end());
    std::sort(exited.begin(), exited.end());
    overlapping_ = std::move(current);

    for (PhysicsBodyId id : entered)
    {
        if (bodyEntered_) { bodyEntered_(id); }
    }
    for (PhysicsBodyId id : exited)
    {
        if (bodyExited_) { bodyExited_(id); }
    }
}
