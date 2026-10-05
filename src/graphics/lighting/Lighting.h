#pragma once

#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

// 单方向光兼容描述。新场景使用SceneLighting；其中主灯仍使用本结构保存旧接口数据。
// ambient是全场景环境项，不是每盏方向光都应重复累加的贡献。
// 方向表示“光线从光源传播到场景”的方向，
// Shader会取反得到表面指向光源的方向；它不会随物体Transform改变。
struct DirectionalLight
{
    glm::vec3 direction{-0.5f, -1.0f, -0.3f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    glm::vec3 ambient{0.12f};
};

// 在进入OpenGL状态修改前校验光源，避免NaN或零方向把整个物体绘制污染。
void validateDirectionalLight(const DirectionalLight &light);

// 稳定归一化，防止极大/极小但有限的方向向量在求长度时溢出或下溢。
glm::vec3 normalizedLightDirection(const DirectionalLight &light);

// 世界矩阵包含祖先变换；零缩放或不可表示的逆矩阵必须显式拒绝。
glm::mat3 lightingNormalMatrix(const glm::mat4 &world);
