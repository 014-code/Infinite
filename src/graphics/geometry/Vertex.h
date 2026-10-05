#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

// 与Shader的location 0/1/2/3对应；字段名替代手工数第几个float。
// 属性偏移使用offsetof计算，不假设GLM类型一定紧密排列。
struct Vertex
{
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    glm::vec2 uv{0.0f};
    // 局部空间表面方向。光照网格必须填写正确法线，默认+Z只适合朝前的平面。
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    // xyz为切线，w为手性(±1)；0表示没有显式切线，PBR改用片段导数生成局部TBN。
    glm::vec4 tangent{0.0f};
    // 保持原RGB字段兼容旧聚合初始化，第四个颜色分量作为独立顶点属性上传。
    float colorAlpha = 1.0f;
};

struct MeshBounds
{
    // 网格局部空间的轴对齐包围盒，包含所有传入顶点；不含物体Transform。
    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
};
