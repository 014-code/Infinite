#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Camera
{
public:
    enum class ProjectionMode { Perspective, Orthographic };
    // 创建朝向场景中心的透视摄像机
    Camera();

    // 一次设置完整视图并校验，失败时保留原配置。up不能为零或与视线平行。
    void setView(const glm::vec3 &position, const glm::vec3 &target,
        const glm::vec3 &up = glm::vec3(0, 1, 0));
    const glm::vec3 &position() const;
    const glm::vec3 &target() const;
    const glm::vec3 &up() const;
    // fov为角度；透视要求0 < near < far。正交允许负near，但仍要求near < far。
    void setPerspective(float fieldOfViewDegrees, float nearPlane, float farPlane);
    // height是视口完整的世界空间高度，宽度由aspectRatio推导。
    void setOrthographic(float height, float nearPlane, float farPlane);
    ProjectionMode projectionMode() const;

    // 获取摄像机视图矩阵
    glm::mat4 viewMatrix() const;

    // 根据窗口宽高比生成透视投影矩阵
    glm::mat4 projectionMatrix(float aspectRatio) const;

private:
    glm::vec3 position_{0.0f, 0.0f, 3.0f};
    glm::vec3 target_{0.0f, 0.0f, 0.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};
    float fieldOfView_ = 45.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 100.0f;
    float orthographicHeight_ = 2.0f;
    ProjectionMode projectionMode_ = ProjectionMode::Perspective;
};
