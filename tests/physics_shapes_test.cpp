#include "TestSupport.h"
#include "physics/shapes/CollisionShape.h"

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
        // 合法构造保留原始参数。
        const SphereShape sphere(2.0f);
        requireNear(sphere.radius, 2.0f, "Sphere radius was not stored");
        const BoxShape box(glm::vec3(1.0f, 2.0f, 3.0f));
        requireNear(box.halfExtents.y, 2.0f, "Box half extents were not stored");
        const CapsuleShape capsule(0.5f, 2.0f);
        requireNear(capsule.cylinderHeight, 2.0f, "Capsule cylinder height was not stored");
        const PlaneShape plane(glm::vec3(0.0f, 2.0f, 0.0f), 1.5f);
        requireNear(glm::length(plane.normal), 1.0f, "Plane normal was not normalized");
        requireNear(plane.normal.y, 1.0f, "Plane normal direction changed");
        requireNear(plane.offset, 1.5f, "Plane offset was not stored");

        // 非法尺寸和数值在构造时拒绝。
        expectThrow<std::invalid_argument>([] { SphereShape(0.0f); }, "Zero sphere radius was accepted");
        expectThrow<std::invalid_argument>([] { SphereShape(-1.0f); }, "Negative sphere radius was accepted");
        expectThrow<std::invalid_argument>([] { SphereShape(std::numeric_limits<float>::quiet_NaN()); },
            "NaN sphere radius was accepted");
        expectThrow<std::invalid_argument>([] { BoxShape(glm::vec3(1.0f, 0.0f, 1.0f)); },
            "Zero box axis was accepted");
        expectThrow<std::invalid_argument>([] { BoxShape(glm::vec3(-1.0f, 1.0f, 1.0f)); },
            "Negative box axis was accepted");
        expectThrow<std::invalid_argument>([] { BoxShape(glm::vec3(1.0f, std::numeric_limits<float>::infinity(), 1.0f)); },
            "Infinite box axis was accepted");
        expectThrow<std::invalid_argument>([] { CapsuleShape(1.0f, -0.5f); },
            "Negative capsule cylinder height was accepted");
        expectThrow<std::invalid_argument>([] { CapsuleShape(-1.0f, 1.0f); },
            "Negative capsule radius was accepted");
        expectThrow<std::invalid_argument>([] { PlaneShape(glm::vec3(0.0f), 0.0f); },
            "Zero plane normal was accepted");
        expectThrow<std::invalid_argument>([] { PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), std::numeric_limits<float>::quiet_NaN()); },
            "NaN plane offset was accepted");

        // 各形状的局部包围盒。
        const CollisionShape sphereShape(sphere);
        const Aabb sphereBox = sphereShape.localAabb();
        requireNear(sphereBox.min.x, -2.0f, "Sphere AABB min is wrong");
        requireNear(sphereBox.max.z, 2.0f, "Sphere AABB max is wrong");

        const CollisionShape boxShape(box);
        const Aabb boxBox = boxShape.localAabb();
        requireNear(boxBox.min.y, -2.0f, "Box AABB min is wrong");
        requireNear(boxBox.max.z, 3.0f, "Box AABB max is wrong");

        // 胶囊总高度 = 圆柱段 + 两端半径；圆柱段为0时退化为球体包围盒。
        const CollisionShape capsuleShape(capsule);
        const Aabb capsuleBox = capsuleShape.localAabb();
        requireNear(capsuleBox.min.y, -1.5f, "Capsule AABB min Y is wrong");
        requireNear(capsuleBox.max.x, 0.5f, "Capsule AABB max X is wrong");
        const CollisionShape degenerateCapsule(CapsuleShape(1.0f, 0.0f));
        const Aabb degenerateBox = degenerateCapsule.localAabb();
        requireNear(degenerateBox.max.y, 1.0f, "Zero-height capsule did not degrade to a sphere AABB");

        // 平面没有有限包围盒，必须显式报错而不是返回假数据。
        const CollisionShape planeShape(plane);
        require(!planeShape.isFinite(), "Plane reported a finite shape");
        require(sphereShape.isFinite(), "Sphere reported a non-finite shape");
        expectThrow<std::logic_error>([&] { planeShape.localAabb(); },
            "Plane localAabb did not reject the call");

        // 值类型可安全复制，类型查询与取回一致。
        const CollisionShape copied = capsuleShape;
        require(copied.holds<CapsuleShape>(), "Copied shape lost its type");
        require(!copied.holds<SphereShape>(), "Shape type query is wrong");
        requireNear(copied.get<CapsuleShape>().radius, 0.5f, "Copied shape lost its data");

        std::cout << "physics_shapes_test passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "physics_shapes_test failed: " << error.what() << '\n';
        return 1;
    }
}
