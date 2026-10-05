#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <cstdint>
#include <cstddef>
#include <vector>

// 静态Vertex不携带骨骼数据；只有蒙皮网格另建这一组顶点属性。
struct SkinVertex
{
    glm::uvec4 joints{0};
    glm::vec4 weights{1, 0, 0, 0};
};

// 共享资源中的关节索引指向Model.nodes；inverseBind与joints逐项对应。
// inverseBind把网格绑定姿态的顶点带到关节空间，不能当成关节当前的逆世界矩阵。
struct Skin
{
    std::vector<std::size_t> joints;
    std::vector<glm::mat4> inverseBind;
};

// 纯CPU数学入口。输出仍在网格局部空间，之后Shader会统一乘一次model。
// 对非有限/不可逆meshWorld明确报错，避免NaN进入GPU。
std::vector<glm::mat4> buildSkinPalette(const glm::mat4 &meshWorld,
    const std::vector<glm::mat4> &jointWorld, const std::vector<glm::mat4> &inverseBind);
