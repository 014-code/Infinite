#pragma once

#include "physics/math/Aabb.h"
#include "physics/shapes/CollisionShape.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <optional>

// 一次形状对形状接触的结果。
struct ShapeContact
{
    // 世界空间接触点：有限形状取形状B上最靠近A的点；平面取形状侧深入平面最深的点。
    glm::vec3 point;
    // 世界空间单位法线，含义见ShapeCollision::collide的约定说明。
    glm::vec3 normal;
    // 相互穿透深度，大于0才构成接触。
    float penetration;
};

// 首版narrowphase：只覆盖静态世界需要的形状组合，不做通用GJK/EPA。
//
// 支持：平面 vs {球体, 盒体, 胶囊}、球体 vs 球体、球体 vs 盒体（盒体可旋转）、
//       胶囊 vs 盒体（竖直胶囊按局部轴向参与，盒体可旋转）。
// 不支持：盒体 vs 盒体、胶囊 vs 球体、胶囊 vs 胶囊、平面 vs 平面。
// 未支持的组合一律返回std::nullopt，不返回近似结果；需要时应先扩展本文件与测试。
namespace ShapeCollision
{
    // 法线约定：
    // - 两个有限形状之间：normal从A指向B，沿+normal移动B（或沿-normal移动A）可解除穿透。
    // - 平面参与时：normal固定为平面朝向正侧的法线（平面用dot(normal, x) = offset描述），
    //   沿+normal把对方推回正侧即可解除穿透。平面没有内部方向，因此不做"A指向B"的推导。
    // 其余情况（不相交、未支持的组合）返回std::nullopt。
    std::optional<ShapeContact> collide(const CollisionShape &shapeA, const glm::vec3 &positionA,
        const glm::quat &rotationA, const CollisionShape &shapeB, const glm::vec3 &positionB,
        const glm::quat &rotationB);

    bool intersects(const CollisionShape &shapeA, const glm::vec3 &positionA, const glm::quat &rotationA,
        const CollisionShape &shapeB, const glm::vec3 &positionB, const glm::quat &rotationB);

    // 世界变换：静态体只支持位置和旋转，不支持缩放；缩放需要重新推导形状参数。
    glm::mat4 composeTransform(const glm::vec3 &position, const glm::quat &rotation);

    // 归一化四元数，零长度或含NaN/无穷大时抛std::invalid_argument。
    // 物理模块内所有位姿都走这一条规则：调用方不要自己写 rotation / length(rotation)，
    // 那样会在校验之前先算出NaN，且各处规则容易漂移。
    glm::quat normalizedRotation(const glm::quat &rotation);

    // 把对方推离接触面的方向。平面用平面正侧法线；其余形状取接触法线的反向
    // （ShapeCollision的法线约定是"从A指向B"）。查询与接触求解共用这一条规则。
    glm::vec3 separationDirection(const ShapeContact &contact, bool otherIsPlane);

    // 形状的世界空间包围盒；平面没有有限包围盒，调用抛std::logic_error。
    Aabb worldAabb(const CollisionShape &shape, const glm::vec3 &position, const glm::quat &rotation);

    // 平面形状在世界空间的法线与offset：满足dot(normal, x) = offset。
    void planeToWorld(const PlaneShape &plane, const glm::vec3 &position, const glm::quat &rotation,
        glm::vec3 &outNormal, float &outOffset);

    // 形状在给定方向（无需预先归一化）上最远的支撑点，世界空间。平面不支持，抛std::invalid_argument。
    glm::vec3 supportWorld(const CollisionShape &shape, const glm::vec3 &position, const glm::quat &rotation,
        const glm::vec3 &direction);
}
