#pragma once

#include "physics/math/Aabb.h"
#include "physics/math/Ray.h"

#include <glm/vec3.hpp>

#include <optional>

// 单次射线求交的结果：t是沿归一化方向的距离，point和normal由t和形状导出。
struct RaycastHit
{
    float t;
    glm::vec3 point;
    glm::vec3 normal;
};

// 求交约定：
// - 起点在形状内部时立即命中，t=0、命中点为射线起点、法线取入射方向的反方向，
//   避免调用方卡在"本应离开却没有出口信息"的状态。
// - t<0的命中视为未命中（命中点在射线背后）。
// - 平面是双面的，命中法线始终朝向射线来向。
// - 所有形状在各自的局部空间描述；世界空间求交由调用方先把射线变换进局部空间。
namespace PhysicsRaycast
{
    std::optional<RaycastHit> intersectSphere(const Ray &ray, const glm::vec3 &center, float radius);
    std::optional<RaycastHit> intersectBox(const Ray &ray, const Aabb &box);
    std::optional<RaycastHit> intersectPlane(const Ray &ray, const glm::vec3 &normal, float offset);
    // 竖直Y轴胶囊，中心位于原点，参数含义与CapsuleShape一致。
    std::optional<RaycastHit> intersectCapsule(const Ray &ray, float radius, float cylinderHeight);
}
