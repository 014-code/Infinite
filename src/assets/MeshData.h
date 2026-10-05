#pragma once

#include "graphics/geometry/Vertex.h"

#include <cstdint>
#include <vector>

// MeshData是CPU侧的网格资产，不包含VAO、VBO等OpenGL对象。
// MeshLoader负责填充它，Mesh负责把它上传到GPU；两者分开后，解析器可以在无窗口
// 的纯CPU测试中运行，也避免文件格式代码侵入渲染层。
struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};
