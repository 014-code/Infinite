#pragma once

#include <cstdint>

// 碰撞过滤：模仿Godot的碰撞层/掩码约定。
//
// - layer表示"这个物体属于哪些位"（可以同时属于多位）。
// - mask表示"这个物体愿意与哪些位碰撞"。
// - 两个物体真正碰撞，需要双方的mask都包含对方的layer；单方面同意不算碰撞。
//
// 查询（raycast/overlapShape）只关心"我能检测哪些层"，因此使用一个queryMask参数，
// 不构造临时Body。默认全通，未显式配置时行为与引入过滤前一致。
struct PhysicsFilter
{
    std::uint32_t layer = 1;
    std::uint32_t mask = 0xFFFFFFFFu;
};

// 层与掩码都是32位，位0对应PhysicsLayers::Default。
namespace PhysicsLayers
{
    constexpr std::uint32_t Default = 1u << 0;
    constexpr std::uint32_t Terrain = 1u << 1;
    constexpr std::uint32_t Character = 1u << 2;
    constexpr std::uint32_t Prop = 1u << 3;
}

inline bool filtersInteract(const PhysicsFilter &a, const PhysicsFilter &b)
{
    return (a.mask & b.layer) != 0u && (b.mask & a.layer) != 0u;
}

inline bool filterMatchesQuery(const PhysicsFilter &filter, std::uint32_t queryMask)
{
    return (filter.layer & queryMask) != 0u;
}
