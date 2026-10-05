#pragma once

#include "PrimitiveTypes.h"
#include "assets/MeshData.h"

// 纯CPU生成器：可以在没有窗口和OpenGL上下文的测试中使用。
// 尺寸支持[0.0001, 1000000]，分段数有上限；超界直接报错，不静默截断参数。
class PrimitiveMeshBuilder final
{
public:
    // 校验当前形状使用的字段，并把无关字段恢复默认，保证等价形状命中同一缓存。
    static PrimitiveDescription canonicalize(const PrimitiveDescription &description);
    static MeshData build(const PrimitiveDescription &description);
};
