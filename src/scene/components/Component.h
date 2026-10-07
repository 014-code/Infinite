#pragma once

class GameObject;

// 所有挂在GameObject上的能力组件共享的最小基类。
//
// 当前组件仍由GameObject按强类型成员直接持有，没有引入通用容器或运行时类型查找。
// 基类只统一“组件属于哪个对象”这一条规则，后续增加动画、音频或Collider组件时，
// 不需要重新设计组件的归属关系。
class Component
{
public:
    virtual ~Component() = default;

    // 独立创建组件用于单元测试时owner可能为空；GameObject中的正式组件都会有owner。
    GameObject *owner() noexcept { return owner_; }
    const GameObject *owner() const noexcept { return owner_; }

protected:
    Component() = default;
    explicit Component(GameObject &owner) noexcept : owner_(&owner) {}

private:
    GameObject *owner_ = nullptr;
};
