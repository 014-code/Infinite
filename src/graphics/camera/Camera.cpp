#include "graphics/camera/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <stdexcept>

namespace
{
    // Camera的公开接口允许调用者传入任意浮点参数，因此先在生成矩阵前
    // 做有限性检查。这样错误会在设置相机时暴露，而不是等到渲染结果变黑。
    bool finite(const glm::vec3 &value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    glm::mat4 checkedMatrix(const glm::mat4 &matrix)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (!std::isfinite(matrix[column][row]))
                {
                    throw std::invalid_argument("Camera parameters produced a non-finite matrix");
                }
            }
        }
        return matrix;
    }
}

Camera::Camera() = default;

void Camera::setView(const glm::vec3 &position, const glm::vec3 &target, const glm::vec3 &up)
{
    // lookAt需要两个条件：位置和目标不能重合，同时视线方向不能与up平行。
    // side向量正是这两个条件的叉积，长度为0时无法建立稳定的相机坐标系。
    const auto direction = target - position;
    const auto side = glm::cross(direction, up);
    if (!finite(position) || !finite(target) || !finite(up) || !finite(direction) || !finite(side) ||
        glm::length(direction) <= 0.0f || glm::length(side) <= 0.0f)
    {
        throw std::invalid_argument("Camera requires a finite, nondegenerate view");
    }
    checkedMatrix(glm::lookAt(position, target, up));
    position_ = position;
    target_ = target;
    up_ = up;
}

const glm::vec3 &Camera::position() const { return position_; }
const glm::vec3 &Camera::target() const { return target_; }
const glm::vec3 &Camera::up() const { return up_; }
Camera::ProjectionMode Camera::projectionMode() const { return projectionMode_; }

void Camera::setPerspective(float fieldOfViewDegrees, float nearPlane, float farPlane)
{
    // 这里先用宽高比1生成一次矩阵，只是验证FOV和裁剪面能产生有效结果。
    // 真正绘制时会在projectionMatrix中使用窗口当前的宽高比。
    if (!std::isfinite(fieldOfViewDegrees) || fieldOfViewDegrees <= 0 || fieldOfViewDegrees >= 180 ||
        !std::isfinite(nearPlane) || !std::isfinite(farPlane) || nearPlane <= 0 || farPlane <= nearPlane)
    {
        throw std::invalid_argument("Perspective requires 0 < fov < 180 and 0 < near < far");
    }
    checkedMatrix(glm::perspective(glm::radians(fieldOfViewDegrees), 1.0f, nearPlane, farPlane));
    fieldOfView_ = fieldOfViewDegrees;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
    projectionMode_ = ProjectionMode::Perspective;
}

void Camera::setOrthographic(float height, float nearPlane, float farPlane)
{
    // 正交投影的height表示可见区域高度，宽度由每帧传入的aspectRatio计算。
    if (!std::isfinite(height) || height <= 0 || !std::isfinite(nearPlane) ||
        !std::isfinite(farPlane) || farPlane <= nearPlane)
    {
        throw std::invalid_argument("Orthographic requires positive height and near < far");
    }
    const float half = height * 0.5f;
    checkedMatrix(glm::ortho(-half, half, -half, half, nearPlane, farPlane));
    orthographicHeight_ = height;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
    projectionMode_ = ProjectionMode::Orthographic;
}

glm::mat4 Camera::viewMatrix() const
{
    // 将世界坐标变换到摄像机视角。Camera只保存参数，不缓存矩阵，
    // 因为调用者可能在绘制前改变窗口比例或相机位置。
    return glm::lookAt(position_, target_, up_);
}

glm::mat4 Camera::projectionMatrix(float aspectRatio) const
{
    if (!std::isfinite(aspectRatio) || aspectRatio <= 0)
    {
        throw std::invalid_argument("Camera aspect ratio must be positive and finite");
    }
    if (projectionMode_ == ProjectionMode::Orthographic)
    {
        const float halfHeight = orthographicHeight_ * 0.5f;
        const float halfWidth = halfHeight * aspectRatio;
        return checkedMatrix(glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane_, farPlane_));
    }
    // 将摄像机空间坐标投影到屏幕；透视投影会让远处物体看起来更小。
    return checkedMatrix(glm::perspective(
        glm::radians(fieldOfView_),
        aspectRatio,
        nearPlane_,
        farPlane_));
}
