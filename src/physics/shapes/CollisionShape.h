#pragma once

#include "physics/math/Aabb.h"

#include <glm/vec3.hpp>

#include <variant>

// 球体碰撞形状：中心始终位于形状局部原点。
struct SphereShape
{
    float radius;

    // 半径必须为正且有限，否则抛std::invalid_argument。
    explicit SphereShape(float radius);
};

// 轴对齐盒体：以局部原点为中心，halfExtents为各轴正方向半边长。
struct BoxShape
{
    glm::vec3 halfExtents;

    // 各轴半边长必须为正且有限，否则抛std::invalid_argument。
    explicit BoxShape(const glm::vec3 &halfExtents);
};

// 竖直Y轴胶囊：cylinderHeight是不含两端半球帽的圆柱段高度，为0时退化为球体。
// 总高度为cylinderHeight + 2 * radius。首版不支持其他轴向的胶囊。
struct CapsuleShape
{
    float radius;
    float cylinderHeight;

    // 半径必须为正；圆柱段高度允许为0但不允许为负，否则抛std::invalid_argument。
    CapsuleShape(float radius, float cylinderHeight);
};

// 无限平面：法线在构造时归一化，满足dot(normal, x) = offset的点位于平面上。
// 平面没有有限包围盒，只用于静态世界几何（地面、无限墙）。
struct PlaneShape
{
    glm::vec3 normal;
    float offset;

    // 零法线或含NaN/无穷大的输入抛std::invalid_argument。
    PlaneShape(const glm::vec3 &normal, float offset);
};

// 碰撞形状是上述形状的值类型联合体，可安全复制；形状自身不携带位姿，
// 位姿由引用形状的Body或调用方提供。
class CollisionShape
{
public:
    CollisionShape(const SphereShape &sphere);
    CollisionShape(const BoxShape &box);
    CollisionShape(const CapsuleShape &capsule);
    CollisionShape(const PlaneShape &plane);

    // 形状在局部空间的包围盒；平面没有有限包围盒，调用抛std::logic_error，
    // 调用前用holds<PlaneShape>()或isFinite()判断。
    Aabb localAabb() const;
    bool isFinite() const;

    template<class T>
    bool holds() const { return std::holds_alternative<T>(data_); }

    template<class T>
    const T &get() const { return std::get<T>(data_); }

private:
    std::variant<SphereShape, BoxShape, CapsuleShape, PlaneShape> data_;
};
