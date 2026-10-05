#include "TestSupport.h"
#include "physics/math/Raycast.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    void requireNear(float actual, float expected, const char *message)
    {
        require(std::abs(actual - expected) < 0.0001f, message);
    }

    void requireVecNear(const glm::vec3 &actual, const glm::vec3 &expected, const char *message)
    {
        requireNear(actual.x, expected.x, message);
        requireNear(actual.y, expected.y, message);
        requireNear(actual.z, expected.z, message);
    }
}

int main()
{
    try
    {
        // 球体：正面命中、命中点与法线、未命中、背向、起点在内部。
        const Ray atSphere(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        const auto sphereHit = PhysicsRaycast::intersectSphere(atSphere, glm::vec3(0.0f), 1.0f);
        require(sphereHit.has_value(), "Ray missed a sphere dead ahead");
        requireNear(sphereHit->t, 4.0f, "Sphere hit distance is wrong");
        requireVecNear(sphereHit->point, glm::vec3(0.0f, 0.0f, 1.0f), "Sphere hit point is wrong");
        requireVecNear(sphereHit->normal, glm::vec3(0.0f, 0.0f, 1.0f), "Sphere hit normal is wrong");

        const Ray beside(glm::vec3(2.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        require(!PhysicsRaycast::intersectSphere(beside, glm::vec3(0.0f), 1.0f).has_value(),
            "Ray hit a sphere it passes beside");
        const Ray away(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        require(!PhysicsRaycast::intersectSphere(away, glm::vec3(0.0f), 1.0f).has_value(),
            "Ray hit a sphere behind it");
        const Ray insideSphere(glm::vec3(0.0f, 0.0f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f));
        const auto insideHit = PhysicsRaycast::intersectSphere(insideSphere, glm::vec3(0.0f), 1.0f);
        require(insideHit.has_value() && insideHit->t == 0.0f, "Origin inside sphere did not hit immediately");
        requireVecNear(insideHit->normal, glm::vec3(-1.0f, 0.0f, 0.0f),
            "Inside hit normal must oppose the ray");

        // 盒体：轴向命中、斜向命中、slab外平行未命中、盒在射线背后、起点在内部。
        const Aabb unitBox = Aabb::fromCenterHalfExtents(glm::vec3(0.0f), glm::vec3(1.0f));
        const Ray atBox(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
        const auto boxHit = PhysicsRaycast::intersectBox(atBox, unitBox);
        require(boxHit.has_value(), "Ray missed a box dead ahead");
        requireNear(boxHit->t, 4.0f, "Box hit distance is wrong");
        requireVecNear(boxHit->normal, glm::vec3(1.0f, 0.0f, 0.0f), "Box hit normal is wrong");

        const Ray diagonal(glm::vec3(0.0f, 5.0f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f));
        const auto diagonalHit = PhysicsRaycast::intersectBox(diagonal, unitBox);
        require(diagonalHit.has_value() && diagonalHit->t == 4.0f, "Diagonal box hit is wrong");
        requireVecNear(diagonalHit->normal, glm::vec3(0.0f, 1.0f, 0.0f), "Diagonal box normal is wrong");

        const Ray outsideSlab(glm::vec3(3.0f, 5.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        require(!PhysicsRaycast::intersectBox(outsideSlab, unitBox).has_value(),
            "Ray outside the slab hit the box");
        require(!PhysicsRaycast::intersectBox(away, unitBox).has_value(), "Box behind the ray was hit");
        const Ray insideBox(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        const auto insideBoxHit = PhysicsRaycast::intersectBox(insideBox, unitBox);
        require(insideBoxHit.has_value() && insideBoxHit->t == 0.0f,
            "Origin inside box did not hit immediately");

        // 平面：双面命中、法线朝来向、平行未命中、背面未命中。
        const glm::vec3 floorNormal(0.0f, 1.0f, 0.0f);
        const Ray downward(glm::vec3(2.0f, 5.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        const auto floorHit = PhysicsRaycast::intersectPlane(downward, floorNormal, 0.0f);
        require(floorHit.has_value(), "Ray missed the floor plane");
        requireNear(floorHit->t, 5.0f, "Floor hit distance is wrong");
        requireVecNear(floorHit->normal, glm::vec3(0.0f, 1.0f, 0.0f), "Floor hit normal must face the ray");

        const Ray upward(glm::vec3(0.0f, -3.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        const auto backHit = PhysicsRaycast::intersectPlane(upward, floorNormal, 0.0f);
        require(backHit.has_value(), "Plane must be double-sided");
        requireVecNear(backHit->normal, glm::vec3(0.0f, -1.0f, 0.0f), "Back-side normal must face the ray");

        const Ray parallel(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        require(!PhysicsRaycast::intersectPlane(parallel, floorNormal, 0.0f).has_value(),
            "Parallel ray hit the plane");
        require(!PhysicsRaycast::intersectPlane(upward, floorNormal, -5.0f).has_value(),
            "Plane behind the ray was hit");

        // 偏移平面：dot(normal, x) = offset。
        const Ray raised(glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        const auto raisedHit = PhysicsRaycast::intersectPlane(raised, floorNormal, 2.0f);
        require(raisedHit.has_value() && raisedHit->t == 3.0f, "Offset plane hit is wrong");
        requireVecNear(raisedHit->point, glm::vec3(0.0f, 2.0f, 0.0f), "Offset plane point is wrong");

        // 胶囊：侧面命中、球帽命中、上方掠过未命中、起点在内部。r=1、圆柱段2 → 总高4。
        const Ray atSide(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
        const auto sideHit = PhysicsRaycast::intersectCapsule(atSide, 1.0f, 2.0f);
        require(sideHit.has_value(), "Ray missed the capsule side");
        requireNear(sideHit->t, 4.0f, "Capsule side hit distance is wrong");
        requireVecNear(sideHit->normal, glm::vec3(1.0f, 0.0f, 0.0f), "Capsule side normal is wrong");

        const Ray atCap(glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        const auto capHit = PhysicsRaycast::intersectCapsule(atCap, 1.0f, 2.0f);
        require(capHit.has_value(), "Ray missed the capsule cap");
        requireNear(capHit->t, 3.0f, "Capsule cap hit distance is wrong");
        requireVecNear(capHit->point, glm::vec3(0.0f, 2.0f, 0.0f), "Capsule cap point is wrong");
        requireVecNear(capHit->normal, glm::vec3(0.0f, 1.0f, 0.0f), "Capsule cap normal is wrong");

        const Ray aboveCapsule(glm::vec3(5.0f, 3.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
        require(!PhysicsRaycast::intersectCapsule(aboveCapsule, 1.0f, 2.0f).has_value(),
            "Ray above the capsule hit it");
        const Ray insideCapsule(glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        const auto insideCapsuleHit = PhysicsRaycast::intersectCapsule(insideCapsule, 1.0f, 2.0f);
        require(insideCapsuleHit.has_value() && insideCapsuleHit->t == 0.0f,
            "Origin inside capsule did not hit immediately");

        std::cout << "physics_raycast_test passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "physics_raycast_test failed: " << error.what() << '\n';
        return 1;
    }
}
