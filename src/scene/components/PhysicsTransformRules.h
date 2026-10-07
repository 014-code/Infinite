#pragma once

#include "math/Transform.h"

// 物理组件当前使用世界空间位姿，而Transform默认还支持父子层级和缩放。
// 这组规则集中在一个小模块中，避免静态体、动态体和角色组件各自复制校验逻辑。
namespace PhysicsTransformRules
{
    // 物理形状的尺寸已经写入CollisionShape，因此组件阶段只接受单位缩放。
    // 同时拒绝NaN和无穷大，避免NaN绕过普通的绝对误差比较。
    bool hasUnitScale(const Transform &transform) noexcept;

    // 检查Transform是否能直接作为物理世界中的世界空间位姿。
    // 失败抛std::invalid_argument，并说明是层级还是缩放不受支持。
    void validate(const Transform &transform);
}
