#include "TestSupport.h"
#include "physics/world/PhysicsWorld.h"

#include <glm/gtc/quaternion.hpp>

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    const glm::quat NO_ROTATION(1.0f, 0.0f, 0.0f, 0.0f);

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
        PhysicsWorld world;
        require(world.bodyCount() == 0, "New world is not empty");

        // 句柄从1开始递增，删除后不复用。
        const CollisionShape ground(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f));
        const CollisionShape sphere(SphereShape(1.0f));
        const CollisionShape box(BoxShape(glm::vec3(1.0f)));

        const PhysicsBodyId groundId = world.createStaticBody(ground, glm::vec3(0.0f));
        const PhysicsBodyId sphereId = world.createStaticBody(sphere, glm::vec3(0.0f, 5.0f, 0.0f));
        const PhysicsBodyId boxId = world.createStaticBody(box, glm::vec3(10.0f, 0.0f, 0.0f));
        require(groundId >= 1 && sphereId == groundId + 1 && boxId == sphereId + 1,
            "Body ids are not assigned in order");
        require(world.contains(sphereId), "Registered body was not found");
        require(!world.contains(0), "Invalid id was accepted");
        require(world.bodyCount() == 3, "Body count is wrong");
        require(world.bodyShape(sphereId) != nullptr, "Body shape lookup failed");
        require(world.bodyShape(999) == nullptr, "Unknown id returned a shape");

        require(!world.destroyBody(999), "Destroying an unknown id reported success");
        require(world.destroyBody(boxId), "Destroying a registered body failed");
        require(world.bodyCount() == 2, "Body count after destroy is wrong");
        require(!world.contains(boxId), "Destroyed body is still present");
        const PhysicsBodyId nextId = world.createStaticBody(box, glm::vec3(20.0f, 0.0f, 0.0f));
        require(nextId > boxId, "Destroyed id was reused");

        // 平面体没有有限包围盒；有限形状可以取回世界AABB。
        require(!world.bodyWorldAabb(groundId).has_value(), "Plane body reported a finite AABB");
        require(world.bodyWorldAabb(sphereId).has_value(), "Sphere body has no world AABB");
        require(world.bodyWorldMatrix(sphereId).has_value(), "Sphere body has no world matrix");
        require(!world.bodyWorldMatrix(999).has_value(), "Unknown id returned a world matrix");

        // 射线检测：球体比地面更近，命中球体；t在刚体变换下保持世界距离。
        const Ray fromAbove(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        const auto nearest = world.raycast(fromAbove);
        require(nearest.has_value(), "Raycast from above hit nothing");
        require(nearest->body == sphereId, "Raycast did not return the nearest body");
        requireNear(nearest->hit.t, 4.0f, "Raycast distance is wrong");
        requireVecNear(nearest->hit.normal, glm::vec3(0.0f, 1.0f, 0.0f), "Raycast normal is wrong");
        // 只测地面：球体被移除后命中平面。
        require(world.destroyBody(sphereId), "Destroying the sphere failed");
        const auto groundOnly = world.raycast(fromAbove);
        require(groundOnly.has_value() && groundOnly->body == groundId, "Raycast did not fall through to the plane");
        requireNear(groundOnly->hit.t, 10.0f, "Plane hit distance is wrong");
        // maxDistance过滤：距离不足时不返回结果。
        require(!world.raycast(fromAbove, 5.0f).has_value(), "maxDistance did not reject a far hit");
        require(world.raycast(fromAbove, 10.0f).has_value(), "maxDistance rejected an in-range hit");
        expectThrow<std::invalid_argument>([&] { world.raycast(fromAbove, -1.0f); },
            "Negative maxDistance was accepted");
        const Ray sideways(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        require(!world.raycast(sideways).has_value(), "Ray parallel to the plane hit something");

        // 移动静态体位姿：移开后不再命中，移回后恢复。
        require(world.setBodyTransform(groundId, glm::vec3(0.0f, -20.0f, 0.0f), NO_ROTATION),
            "Moving a body failed");
        require(!world.raycast(fromAbove, 15.0f).has_value(), "Moved plane was still hit");
        require(!world.setBodyTransform(999, glm::vec3(0.0f), NO_ROTATION),
            "Moving an unknown body reported success");
        require(world.setBodyTransform(groundId, glm::vec3(0.0f), NO_ROTATION), "Restoring the body failed");
        require(world.raycast(fromAbove).has_value(), "Restored plane was not hit");

        // 非法位姿必须在写入前被拒绝，不能留下只更新了一半的Body状态。
        const glm::vec3 invalidPosition(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f);
        expectThrow<std::invalid_argument>([&] {
            world.setBodyTransform(groundId, invalidPosition, NO_ROTATION); },
            "NaN body position was accepted");
        requireVecNear(*world.bodyPosition(groundId), glm::vec3(0.0f),
            "Failed body transform update changed the old position");

        // 重叠查询：命中盒体并返回接触信息。地面是无限平面，抬高查询球到墙顶高度后只应命中墙体。
        const PhysicsBodyId wallId = world.createStaticBody(box, glm::vec3(0.0f, 0.0f, 0.0f));
        const auto overlaps = world.overlapShape(CollisionShape(SphereShape(0.5f)), glm::vec3(1.2f, 1.0f, 0.0f),
            NO_ROTATION);
        require(overlaps.size() == 1, "Overlap query returned the wrong number of bodies");
        require(overlaps.front().body == wallId, "Overlap query returned the wrong body");
        requireNear(overlaps.front().contact.penetration, 0.3f, "Overlap penetration is wrong");
        // 与地面同高时，无限平面同样参与重叠查询。
        const auto onGround = world.overlapShape(CollisionShape(SphereShape(0.5f)), glm::vec3(1.2f, 0.0f, 0.0f),
            NO_ROTATION);
        require(onGround.size() == 2, "Overlap query on the ground returned the wrong number of bodies");
        bool foundWall = false;
        bool foundGround = false;
        for (const auto &result : onGround)
        {
            foundWall = foundWall || result.body == wallId;
            foundGround = foundGround || result.body == groundId;
        }
        require(foundWall && foundGround, "Overlap query did not return both the wall and the plane");
        // 包围盒不相交的物体不会进入精确求交；查询球离开地面平面高度才是真正"远处"。
        const auto distant = world.overlapShape(CollisionShape(SphereShape(0.5f)), glm::vec3(50.0f, 5.0f, 0.0f),
            NO_ROTATION);
        require(distant.empty(), "Overlap query hit a distant body");
        require(!world.overlapShape(CollisionShape(SphereShape(0.5f)), glm::vec3(50.0f, 0.0f, 0.0f),
            NO_ROTATION).empty(), "Infinite ground plane must be reported when overlapping");
        expectThrow<std::invalid_argument>([&] {
            world.overlapShape(ground, glm::vec3(0.0f), NO_ROTATION); },
            "Plane overlap query was accepted");

        // broadphase：相交与相切都进入候选对，分离的盒不产生候选对，平面不参与。
        PhysicsWorld broadphaseWorld;
        const PhysicsBodyId left = broadphaseWorld.createStaticBody(box, glm::vec3(0.0f));
        const PhysicsBodyId right = broadphaseWorld.createStaticBody(box, glm::vec3(1.5f, 0.0f, 0.0f));
        const PhysicsBodyId touching = broadphaseWorld.createStaticBody(box, glm::vec3(2.0f, 0.0f, 0.0f));
        const PhysicsBodyId planeId = broadphaseWorld.createStaticBody(ground, glm::vec3(0.0f, -5.0f, 0.0f));

        const auto hasPair = [](const std::vector<std::pair<PhysicsBodyId, PhysicsBodyId>> &pairs,
                                 PhysicsBodyId first, PhysicsBodyId second)
        {
            for (const auto &pair : pairs)
            {
                if ((pair.first == first && pair.second == second) ||
                    (pair.first == second && pair.second == first))
                {
                    return true;
                }
            }
            return false;
        };

        // 三个盒两两相邻（left-right、right-touching、left-touching都接触）。
        const auto pairs = broadphaseWorld.broadphasePairs();
        require(pairs.size() == 3, "Broadphase pair count is wrong");
        require(hasPair(pairs, left, right), "Broadphase missed the left-right pair");
        require(hasPair(pairs, right, touching), "Broadphase missed the right-touching pair");
        require(hasPair(pairs, left, touching), "Broadphase missed the left-touching pair");
        // 抬高right后只剩left-touching一对。
        broadphaseWorld.setBodyTransform(right, glm::vec3(1.5f, 10.0f, 0.0f), NO_ROTATION);
        const auto raised = broadphaseWorld.broadphasePairs();
        require(raised.size() == 1 && hasPair(raised, left, touching),
            "Broadphase pairs ignored the Y range");
        // 平面没有有限包围盒，永远不会出现在候选对里。
        for (const auto &pair : raised)
        {
            require(pair.first != planeId && pair.second != planeId, "Plane body appeared in broadphase pairs");
        }

        // 规模参考：1000个互不相交的静态盒不应产生候选对，也不应退化成明显缓慢的实现。
        PhysicsWorld largeWorld;
        for (int index = 0; index < 1000; ++index)
        {
            largeWorld.createStaticBody(box, glm::vec3(static_cast<float>(index) * 4.0f, 0.0f, 0.0f));
        }
        const auto started = std::chrono::steady_clock::now();
        const auto largePairs = largeWorld.broadphasePairs();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        require(largePairs.empty(), "Separated boxes produced broadphase pairs");
        std::cout << "broadphase of 1000 static boxes: " << elapsed << " ms (reference only)\n";

        // id到下标的映射：删除中间物体后下标会移动，其余句柄必须仍解析到正确的数据。
        {
            PhysicsWorld mapWorld;
            const CollisionShape capsule(CapsuleShape(0.5f, 1.0f));
            const PhysicsBodyId firstBody = mapWorld.createStaticBody(box, glm::vec3(0.0f));
            const PhysicsBodyId middleBody = mapWorld.createStaticBody(sphere, glm::vec3(10.0f, 0.0f, 0.0f));
            const PhysicsBodyId lastBody = mapWorld.createStaticBody(capsule, glm::vec3(20.0f, 0.0f, 0.0f));
            require(mapWorld.destroyBody(middleBody), "Destroying the middle body failed");
            require(!mapWorld.contains(middleBody), "Destroyed body is still reachable");
            require(mapWorld.contains(firstBody) && mapWorld.contains(lastBody),
                "Remaining bodies became unreachable after erase");
            require(mapWorld.bodyShape(firstBody) != nullptr &&
                mapWorld.bodyShape(firstBody)->holds<BoxShape>(), "First body shape shifted after erase");
            require(mapWorld.bodyShape(lastBody) != nullptr &&
                mapWorld.bodyShape(lastBody)->holds<CapsuleShape>(), "Last body shape shifted after erase");
            requireVecNear(*mapWorld.bodyPosition(firstBody), glm::vec3(0.0f), "First body position shifted");
            requireVecNear(*mapWorld.bodyPosition(lastBody), glm::vec3(20.0f, 0.0f, 0.0f),
                "Last body position shifted");
            // 删除后再新增，映射继续可用。
            const PhysicsBodyId added = mapWorld.createStaticBody(sphere, glm::vec3(30.0f, 0.0f, 0.0f));
            requireVecNear(*mapWorld.bodyPosition(added), glm::vec3(30.0f, 0.0f, 0.0f),
                "Body added after erase is unreachable");
            const auto ids = mapWorld.bodyIds();
            require(ids == std::vector<PhysicsBodyId>{firstBody, lastBody, added},
                "Body registration order changed after swap-and-pop removal");
            require(mapWorld.bodyCount() == 3, "Body count after erase and add is wrong");
        }

        // clear清空全部物体。
        world.clear();
        require(world.bodyCount() == 0, "World was not cleared");
        require(!world.raycast(fromAbove).has_value(), "Cleared world still reported a hit");

        std::cout << "physics_world_test passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "physics_world_test failed: " << error.what() << '\n';
        return 1;
    }
}
