#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <stdexcept>

// 摄像机相对输入 → 世界空间水平移动方向（示例层共用，不属于引擎库）。
//
// 方向约定：viewDirection是"从摄像机指向观察目标"的视线方向；
// 屏幕右方 = cross(viewDirection, up)。这与FreeCameraController一致：
// 右手系 +Y向上时，视线朝-Z，屏幕右方是+X（朝+Z时右方是-X）。
// 写成cross(up, viewDirection)会得到完全相反的结果（左右颠倒）。
//
// forwardInput对应W/S，rightInput对应D/A；返回未归一化的合成向量，
// 由调用方决定是否归一化（保留"输入强度小于1时按强度移动"的语义）。
// 视线投影到水平面后长度为零（完全垂直俯视）或输入非有限时抛std::invalid_argument。
inline glm::vec3 cameraRelativeDirection(const glm::vec3 &viewDirection, float forwardInput, float rightInput)
{
    if (!std::isfinite(forwardInput) || !std::isfinite(rightInput))
    {
        throw std::invalid_argument("Camera-relative input must be finite");
    }
    const glm::vec3 flat(viewDirection.x, 0.0f, viewDirection.z);
    const float length = glm::length(flat);
    if (!std::isfinite(length) || length <= 0.0f)
    {
        throw std::invalid_argument("Camera-relative movement needs a non-vertical view direction");
    }
    const glm::vec3 forward = flat / length;
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    return forward * forwardInput + right * rightInput;
}
