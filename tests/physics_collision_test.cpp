#include "TestSupport.h"
#include "physics/world/ShapeCollision.h"

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    const glm::quat NO_ROTATION(1.0f, 0.0f, 0.0f, 0.0f);
    const float QUARTER_TURN = 1.57079632679f;

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

    glm::quat rotationAroundZ(float radians)
    {
        return glm::quat(glm::vec3(0.0f, 0.0f, radians));
    }
}

int main()
{
    try
    {
        const CollisionShape ground(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f));
        const CollisionShape unitSphere(SphereShape(1.0f));
        const CollisionShape unitBox(BoxShape(glm::vec3(1.0f)));
        const CollisionShape capsule(CapsuleShape(0.5f, 2.0f));

        // 平面 vs 球体：穿入深度、正侧无接触、法线朝正侧。
        const auto sphereOnPlane = ShapeCollision::collide(ground, glm::vec3(0.0f), NO_ROTATION, unitSphere,
            glm::vec3(0.0f, 0.5f, 0.0f), NO_ROTATION);
        require(sphereOnPlane.has_value(), "Sphere half-buried in the ground was not detected");
        requireNear(sphereOnPlane->penetration, 0.5f, "Sphere plane penetration is wrong");
        requireVecNear(sphereOnPlane->normal, glm::vec3(0.0f, 1.0f, 0.0f), "Plane normal must point to the front side");
        requireVecNear(sphereOnPlane->point, glm::vec3(0.0f, -0.5f, 0.0f), "Deepest sphere point is wrong");
        require(!ShapeCollision::intersects(ground, glm::vec3(0.0f), NO_ROTATION, unitSphere,
            glm::vec3(0.0f, 2.0f, 0.0f), NO_ROTATION), "Sphere above the ground was reported as touching");

        // 平面 vs 盒体与胶囊：沿-法线的支撑点决定穿透深度。
        const auto boxOnPlane = ShapeCollision::collide(ground, glm::vec3(0.0f), NO_ROTATION, unitBox,
            glm::vec3(0.0f, 0.5f, 0.0f), NO_ROTATION);
        require(boxOnPlane.has_value(), "Box half-buried in the ground was not detected");
        requireNear(boxOnPlane->penetration, 0.5f, "Box plane penetration is wrong");
        const auto capsuleOnPlane = ShapeCollision::collide(ground, glm::vec3(0.0f), NO_ROTATION, capsule,
            glm::vec3(0.0f, 1.0f, 0.0f), NO_ROTATION);
        require(capsuleOnPlane.has_value(), "Capsule half-buried in the ground was not detected");
        requireNear(capsuleOnPlane->penetration, 0.5f, "Capsule plane penetration is wrong");

        // 旋转后的平面：局部+Y法线绕Z轴旋转90度后指向世界-X。
        const auto rotatedPlaneContact = ShapeCollision::collide(ground, glm::vec3(0.0f), rotationAroundZ(QUARTER_TURN),
            unitSphere, glm::vec3(-0.5f, 0.0f, 0.0f), NO_ROTATION);
        require(rotatedPlaneContact.has_value(), "Rotated plane contact was not detected");
        requireNear(rotatedPlaneContact->penetration, 0.5f, "Rotated plane penetration is wrong");
        requireVecNear(rotatedPlaneContact->normal, glm::vec3(-1.0f, 0.0f, 0.0f),
            "Rotated plane normal is wrong");
        // 平移平面体等价于改变offset。
        const auto offsetPlaneContact = ShapeCollision::collide(ground, glm::vec3(0.0f, 2.0f, 0.0f),
            NO_ROTATION, unitSphere, glm::vec3(0.0f, 3.0f, 0.0f), NO_ROTATION);
        require(offsetPlaneContact.has_value() && std::abs(offsetPlaneContact->penetration) < 0.0001f,
            "Translated plane offset is wrong");

        // 球体 vs 球体：穿透深度、法线方向、相切算接触、分离算未接触。
        const auto spheres = ShapeCollision::collide(unitSphere, glm::vec3(0.0f), NO_ROTATION, unitSphere,
            glm::vec3(1.5f, 0.0f, 0.0f), NO_ROTATION);
        require(spheres.has_value(), "Overlapping spheres were not detected");
        requireNear(spheres->penetration, 0.5f, "Sphere-sphere penetration is wrong");
        requireVecNear(spheres->normal, glm::vec3(1.0f, 0.0f, 0.0f), "Sphere-sphere normal is wrong");
        requireVecNear(spheres->point, glm::vec3(0.75f, 0.0f, 0.0f), "Sphere-sphere contact point is wrong");
        const auto touching = ShapeCollision::collide(unitSphere, glm::vec3(0.0f), NO_ROTATION, unitSphere,
            glm::vec3(2.0f, 0.0f, 0.0f), NO_ROTATION);
        require(touching.has_value() && std::abs(touching->penetration) < 0.0001f,
            "Touching spheres must count as contact");
        require(!ShapeCollision::intersects(unitSphere, glm::vec3(0.0f), NO_ROTATION, unitSphere,
            glm::vec3(2.5f, 0.0f, 0.0f), NO_ROTATION), "Separated spheres were reported as touching");
        // 完全重合时法线无定义，必须返回有限值。
        const auto coincident = ShapeCollision::collide(unitSphere, glm::vec3(0.0f), NO_ROTATION, unitSphere,
            glm::vec3(0.0f), NO_ROTATION);
        require(coincident.has_value(), "Coincident spheres were not detected");
        require(std::isfinite(coincident->normal.y), "Coincident spheres produced a non-finite normal");

        // 球体 vs 盒体：球在盒外侧；法线从球指向盒体。
        const auto sphereOnBox = ShapeCollision::collide(CollisionShape(SphereShape(0.5f)), glm::vec3(1.25f, 0.0f, 0.0f),
            NO_ROTATION, unitBox, glm::vec3(0.0f), NO_ROTATION);
        require(sphereOnBox.has_value(), "Sphere touching a box face was not detected");
        requireNear(sphereOnBox->penetration, 0.25f, "Sphere-box penetration is wrong");
        requireVecNear(sphereOnBox->normal, glm::vec3(-1.0f, 0.0f, 0.0f), "Sphere-box normal must point from sphere to box");
        requireVecNear(sphereOnBox->point, glm::vec3(1.0f, 0.0f, 0.0f), "Sphere-box contact point is wrong");
        // 交换顺序后法线取反。
        const auto boxOnSphere = ShapeCollision::collide(unitBox, glm::vec3(0.0f), NO_ROTATION,
            CollisionShape(SphereShape(0.5f)), glm::vec3(1.25f, 0.0f, 0.0f), NO_ROTATION);
        require(boxOnSphere.has_value(), "Swapped sphere-box order lost the contact");
        requireVecNear(boxOnSphere->normal, glm::vec3(1.0f, 0.0f, 0.0f), "Swapped sphere-box normal is wrong");

        // 球心落在盒内：取最近的面对推出。
        const auto insideBox = ShapeCollision::collide(CollisionShape(SphereShape(0.5f)), glm::vec3(0.0f, 0.9f, 0.0f),
            NO_ROTATION, unitBox, glm::vec3(0.0f), NO_ROTATION);
        require(insideBox.has_value(), "Sphere inside a box was not detected");
        requireNear(insideBox->penetration, 0.6f, "Inside-box penetration is wrong");
        requireVecNear(insideBox->normal, glm::vec3(0.0f, -1.0f, 0.0f), "Inside-box normal must use the nearest face");

        // 旋转盒体：半边长(2,0.5,0.5)绕Z轴旋转90度后沿世界Y伸展2。
        const CollisionShape longBox(BoxShape(glm::vec3(2.0f, 0.5f, 0.5f)));
        const auto rotatedBox = ShapeCollision::collide(CollisionShape(SphereShape(0.5f)), glm::vec3(0.0f, 2.3f, 0.0f),
            NO_ROTATION, longBox, glm::vec3(0.0f), rotationAroundZ(QUARTER_TURN));
        require(rotatedBox.has_value(), "Rotated box contact was not detected");
        requireNear(rotatedBox->penetration, 0.2f, "Rotated box penetration is wrong");
        requireVecNear(rotatedBox->normal, glm::vec3(0.0f, -1.0f, 0.0f), "Rotated box normal is wrong");

        // 胶囊 vs 盒体：角色控制器依赖这一组合（墙、台阶、可站立的盒面）。
        const CollisionShape tallCapsule(CapsuleShape(0.4f, 1.0f)); // 总高1.8
        const auto capsuleOnBoxFace = ShapeCollision::collide(unitBox, glm::vec3(0.0f), NO_ROTATION, tallCapsule,
            glm::vec3(0.0f, 1.8f, 0.0f), NO_ROTATION);
        require(capsuleOnBoxFace.has_value(), "Capsule standing on a box was not detected");
        requireNear(capsuleOnBoxFace->penetration, 0.1f, "Capsule-on-box penetration is wrong");
        requireVecNear(capsuleOnBoxFace->normal, glm::vec3(0.0f, 1.0f, 0.0f),
            "Capsule-on-box normal must point from the box to the capsule");

        const auto capsuleSide = ShapeCollision::collide(unitBox, glm::vec3(0.0f), NO_ROTATION, tallCapsule,
            glm::vec3(1.2f, 0.0f, 0.0f), NO_ROTATION);
        require(capsuleSide.has_value(), "Capsule beside a box was not detected");
        requireNear(capsuleSide->penetration, 0.2f, "Capsule-beside-box penetration is wrong");
        requireVecNear(capsuleSide->normal, glm::vec3(1.0f, 0.0f, 0.0f), "Capsule-beside-box normal is wrong");

        // 远离的胶囊不产生接触；间隔大于半径时必须明确未命中。
        require(!ShapeCollision::intersects(unitBox, glm::vec3(0.0f), NO_ROTATION, tallCapsule,
            glm::vec3(3.0f, 0.0f, 0.0f), NO_ROTATION), "Distant capsule reported a contact");

        // 旋转盒体：半边长(2,0.5,0.5)绕Z轴旋转90度后沿世界Y伸展2，竖直胶囊贴在其侧面。
        const auto rotatedCapsuleBox = ShapeCollision::collide(longBox, glm::vec3(0.0f),
            rotationAroundZ(QUARTER_TURN), tallCapsule, glm::vec3(0.85f, 0.0f, 0.0f), NO_ROTATION);
        require(rotatedCapsuleBox.has_value(), "Rotated box did not hit the capsule");
        requireNear(rotatedCapsuleBox->penetration, 0.05f, "Rotated capsule-box penetration is wrong");

        // 未支持的组合必须明确返回空结果，而不是给近似值。
        require(!ShapeCollision::intersects(unitBox, glm::vec3(0.0f), NO_ROTATION, unitBox, glm::vec3(0.5f),
            NO_ROTATION), "Unsupported box-box pair returned a result");
        require(!ShapeCollision::intersects(capsule, glm::vec3(0.0f), NO_ROTATION, unitSphere, glm::vec3(0.5f),
            NO_ROTATION), "Unsupported capsule-sphere pair returned a result");
        require(!ShapeCollision::intersects(ground, glm::vec3(0.0f), NO_ROTATION, ground, glm::vec3(0.0f),
            NO_ROTATION), "Plane-plane pair returned a result");

        // 世界包围盒：盒体旋转后AABB变大，平面没有有限包围盒。
        const Aabb worldBox = ShapeCollision::worldAabb(unitBox, glm::vec3(1.0f, 0.0f, 0.0f), NO_ROTATION);
        requireNear(worldBox.center().x, 1.0f, "World AABB center is wrong");
        requireNear(worldBox.max.y, 1.0f, "World AABB extent is wrong");
        const Aabb rotatedWorldBox = ShapeCollision::worldAabb(longBox, glm::vec3(0.0f),
            rotationAroundZ(QUARTER_TURN));
        requireNear(rotatedWorldBox.max.y, 2.0f, "Rotated world AABB is wrong");
        expectThrow<std::logic_error>([&] { ShapeCollision::worldAabb(ground, glm::vec3(0.0f), NO_ROTATION); },
            "Plane world AABB did not reject the call");

        // 支撑点：方向归一化由调用方决定，函数内部处理长度。
        const glm::vec3 boxSupport = ShapeCollision::supportWorld(unitBox, glm::vec3(0.0f), NO_ROTATION,
            glm::vec3(0.0f, 5.0f, 0.0f));
        requireVecNear(boxSupport, glm::vec3(1.0f, 1.0f, 1.0f), "Box support point is wrong");
        expectThrow<std::invalid_argument>([&] {
            ShapeCollision::supportWorld(ground, glm::vec3(0.0f), NO_ROTATION, glm::vec3(0.0f, 1.0f, 0.0f)); },
            "Plane support point did not reject the call");

        // 非法旋转在组合变换时拒绝。
        expectThrow<std::invalid_argument>([] {
            ShapeCollision::composeTransform(glm::vec3(0.0f), glm::quat(0.0f, 0.0f, 0.0f, 0.0f)); },
            "Zero quaternion was accepted by composeTransform");

        std::cout << "physics_collision_test passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "physics_collision_test failed: " << error.what() << '\n';
        return 1;
    }
}
