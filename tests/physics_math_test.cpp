#include "TestSupport.h"
#include "physics/math/Aabb.h"
#include "physics/math/ClosestPoint.h"
#include "physics/math/Ray.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    void requireNear(float actual, float expected, const char *message)
    {
        require(std::abs(actual - expected) < 0.0001f, message);
    }
}

int main()
{
    try
    {
        // Aabb构造校验：min不得大于max，数值必须有限。
        expectThrow<std::invalid_argument>([] { Aabb(glm::vec3(1.0f), glm::vec3(0.0f)); },
            "Inverted AABB was accepted");
        expectThrow<std::invalid_argument>([] {
            Aabb(glm::vec3(0.0f), glm::vec3(std::numeric_limits<float>::quiet_NaN())); },
            "NaN AABB was accepted");
        expectThrow<std::invalid_argument>([] {
            Aabb::fromCenterHalfExtents(glm::vec3(0.0f), glm::vec3(-1.0f)); },
            "Negative half extents were accepted");

        const Aabb unit = Aabb::fromCenterHalfExtents(glm::vec3(0.0f), glm::vec3(1.0f));
        requireNear(unit.center().x, 0.0f, "AABB center is wrong");
        requireNear(unit.halfExtents().y, 1.0f, "AABB half extents are wrong");

        // 相交：重叠、边界相切（视为相交）、分离。
        const Aabb shifted = Aabb::fromCenterHalfExtents(glm::vec3(1.5f, 0.0f, 0.0f), glm::vec3(1.0f));
        require(unit.intersects(shifted), "Overlapping AABBs did not intersect");
        const Aabb touching = Aabb::fromCenterHalfExtents(glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        require(unit.intersects(touching), "Touching AABBs must count as intersecting");
        const Aabb apart = Aabb::fromCenterHalfExtents(glm::vec3(2.1f, 0.0f, 0.0f), glm::vec3(1.0f));
        require(!unit.intersects(apart), "Disjoint AABBs intersected");

        // 包含：边界上的点视为被包含。
        require(unit.contains(glm::vec3(1.0f, 0.0f, 0.0f)), "Boundary point was not contained");
        require(!unit.contains(glm::vec3(1.1f, 0.0f, 0.0f)), "Outside point was contained");
        require(unit.contains(touching) == false, "Partially outside AABB was contained");
        require(unit.contains(Aabb::fromCenterHalfExtents(glm::vec3(0.0f), glm::vec3(0.5f))),
            "Inner AABB was not contained");

        // 扩展与合并。
        const Aabb grown = unit.expanded(0.5f);
        requireNear(grown.max.x, 1.5f, "Expanded AABB is wrong");
        const Aabb merged = unit.united(apart);
        requireNear(merged.min.x, -1.0f, "United AABB min is wrong");
        requireNear(merged.max.x, 3.1f, "United AABB max is wrong");

        // 变换：绕Z轴旋转90度后X/Y半边长互换，结果是放大的AABB而非OBB。
        const Aabb elongated = Aabb::fromCenterHalfExtents(glm::vec3(0.0f), glm::vec3(1.0f, 2.0f, 3.0f));
        const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), 1.57079632679f, glm::vec3(0.0f, 0.0f, 1.0f));
        const Aabb rotated = elongated.transformed(rotation);
        requireNear(rotated.halfExtents().x, 2.0f, "Rotated AABB X extent is wrong");
        requireNear(rotated.halfExtents().y, 1.0f, "Rotated AABB Y extent is wrong");
        requireNear(rotated.halfExtents().z, 3.0f, "Rotated AABB Z extent is wrong");
        const glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f));
        requireNear(elongated.transformed(translation).center().x, 5.0f, "Translated AABB is wrong");

        // Ray：方向归一化，非法输入拒绝。
        const Ray ray(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.0f, 0.0f, -5.0f));
        requireNear(glm::length(ray.direction), 1.0f, "Ray direction was not normalized");
        requireNear(ray.direction.z, -1.0f, "Ray direction changed");
        const glm::vec3 at = ray.pointAt(2.0f);
        requireNear(at.x, 1.0f, "Ray pointAt is wrong");
        requireNear(at.z, 1.0f, "Ray pointAt is wrong");
        expectThrow<std::invalid_argument>([] { Ray(glm::vec3(0.0f), glm::vec3(0.0f)); },
            "Zero ray direction was accepted");
        expectThrow<std::invalid_argument>([] {
            Ray(glm::vec3(std::numeric_limits<float>::quiet_NaN()), glm::vec3(1.0f, 0.0f, 0.0f)); },
            "NaN ray origin was accepted");

        // 最近点：线段投影、端点夹紧、退化线段；AABB内部返回自身、外部夹紧。
        const glm::vec3 segA(0.0f, 0.0f, 0.0f);
        const glm::vec3 segB(0.0f, 2.0f, 0.0f);
        requireNear(PhysicsClosestPoint::onSegment(glm::vec3(1.0f, 1.0f, 0.0f), segA, segB).y, 1.0f,
            "Segment projection is wrong");
        requireNear(PhysicsClosestPoint::onSegment(glm::vec3(0.0f, 5.0f, 0.0f), segA, segB).y, 2.0f,
            "Segment clamping is wrong");
        requireNear(PhysicsClosestPoint::onSegment(glm::vec3(3.0f, 4.0f, 0.0f), segA, segA).x, 0.0f,
            "Degenerate segment is wrong");
        const glm::vec3 inside = PhysicsClosestPoint::onAabb(glm::vec3(0.5f, 0.0f, 0.0f), unit);
        requireNear(inside.x, 0.5f, "Point inside AABB was moved");
        const glm::vec3 clamped = PhysicsClosestPoint::onAabb(glm::vec3(3.0f, -2.0f, 0.0f), unit);
        requireNear(clamped.x, 1.0f, "AABB clamping is wrong");
        requireNear(clamped.y, -1.0f, "AABB clamping is wrong");

        std::cout << "physics_math_test passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "physics_math_test failed: " << error.what() << '\n';
        return 1;
    }
}
