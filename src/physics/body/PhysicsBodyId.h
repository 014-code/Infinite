#pragma once

#include <cstdint>

// 物理体句柄从1开始递增，0保留为无效值。
// 句柄类型独立出来，场景组件可以引用它而不需要包含整个PhysicsWorld接口。
using PhysicsBodyId = std::uint32_t;
