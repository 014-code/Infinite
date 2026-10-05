#pragma once

#include "graphics/geometry/primitives/PrimitiveTypes.h"
#include <glm/vec4.hpp>
#include <iosfwd>
#include <optional>

class Renderable;

namespace scene_serialization
{
    // 场景版本4中的纯数据块，不保存指针或OpenGL句柄。
    // 内置材质使用color；文件材质由对象的MATERIAL字段引用，此时忽略color。
    struct PrimitiveState
    {
        PrimitiveDescription geometry;
        glm::vec4 color{1.0f};
    };

    std::optional<PrimitiveState> readPrimitive(std::istream &stream);
    void writePrimitive(std::ostream &stream, const Renderable &renderable);
}
