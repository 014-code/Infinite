#include "TestSupport.h"
#include "physics/world/PhysicsWorld.h"
#include "scene/GameObject.h"
#include "scene/Scene.h"

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    const glm::quat NO_ROTATION(1.0f, 0.0f, 0.0f, 0.0f);
}

int main()
{
    try
    {
        // PhysicsWorld必须在Scene之前声明：物体借用世界，世界必须活得更久。
        PhysicsWorld world;
        const CollisionShape sphere(SphereShape(1.0f));
        const CollisionShape box(BoxShape(glm::vec3(0.5f)));
        const CollisionShape plane(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f));

        // 组件级：注册、查询、同步位姿、重复注册、注销。
        {
            PhysicsBodyComponent component;
            require(!component.isAttached() && component.bodyId() == 0 && component.shape() == nullptr,
                "New component is not empty");
            Transform transform;
            transform.position = glm::vec3(0.0f, 5.0f, 0.0f);
            component.attach(world, sphere, transform.position, transform.rotation());
            require(component.isAttached(), "Component did not register");
            require(world.bodyCount() == 1, "Registered body is missing from the world");
            require(component.shape() != nullptr && component.shape()->holds<SphereShape>(),
                "Component did not keep the shape");
            const PhysicsBodyId firstId = component.bodyId();
            const auto hit = world.raycast(Ray(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
            require(hit.has_value() && hit->body == firstId, "Registered body was not hit");

            // 同步Transform后命中位置随之改变。
            transform.position = glm::vec3(0.0f, 2.0f, 0.0f);
            require(component.syncFromTransform(transform), "Sync from transform failed");
            const auto moved = world.raycast(Ray(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
            require(moved.has_value() && std::abs(moved->hit.t - 7.0f) < 0.0001f,
                "Synced body position is wrong");

            // 重复注册先注销旧体，世界中的体数量不变。
            component.attach(world, box, glm::vec3(3.0f, 0.0f, 0.0f), NO_ROTATION);
            require(world.bodyCount() == 1, "Re-attaching leaked the previous body");
            require(component.bodyId() != firstId, "Re-attaching reused the previous id");
            require(!world.contains(firstId), "Previous body is still registered");

            component.detach();
            require(!component.isAttached() && world.bodyCount() == 0, "Detach did not unregister the body");
            component.detach();
            require(world.bodyCount() == 0, "Repeated detach changed the world");
            require(!component.syncFromTransform(transform), "Detached component accepted a sync");

            // 平面形状属于静态世界几何，不作为物体组件注册。
            expectThrow<std::invalid_argument>([&] { component.attach(world, plane); },
                "Plane shape was accepted as a body component");
            require(world.bodyCount() == 0, "Rejected plane shape left a body behind");
        }
        require(world.bodyCount() == 0, "Component destruction left a registered body");

        // 场景级：物体注册后随物体删除自动注销。
        {
            Scene scene;
            GameObject &wall = scene.createObject("wall");
            wall.transform.position = glm::vec3(2.0f, 0.0f, 0.0f);
            wall.setPhysicsBody(world, box);
            require(wall.physicsBody().isAttached(), "GameObject physics body was not attached");
            require(world.bodyCount() == 1, "GameObject body is missing from the world");
            const PhysicsBodyId wallId = wall.physicsBody().bodyId();
            require(world.contains(wallId), "GameObject body handle is not in the world");

            // 注册位姿取物体当前Transform，不需要额外同步。
            const auto atWall = world.raycast(Ray(glm::vec3(2.0f, 5.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
            require(atWall.has_value() && atWall->body == wallId, "Body was not registered at the object position");

            // 物体移动后同步，碰撞位置随之更新。
            wall.transform.position = glm::vec3(4.0f, 0.0f, 0.0f);
            require(wall.physicsBody().syncFromTransform(wall.transform), "GameObject sync failed");
            require(!world.raycast(Ray(glm::vec3(2.0f, 5.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f))).has_value(),
                "Moved object still collides at the old position");

            // 手动清除后物体仍在场景中，只是不再参与碰撞。
            wall.clearPhysicsBody();
            require(!wall.physicsBody().isAttached() && world.bodyCount() == 0,
                "clearPhysicsBody did not unregister the body");
            wall.setPhysicsBody(world, box);
            const ObjectId wallObjectId = wall.id();
            require(scene.removeObject(wallObjectId), "Removing the object failed");
            require(world.bodyCount() == 0, "Removing the object left a registered body");
            require(!world.contains(wallId), "Removed object body is still registered");

            // Scene::clear同样通过析构注销。
            GameObject &another = scene.createObject("another");
            another.setPhysicsBody(world, sphere);
            require(world.bodyCount() == 1, "Second object body was not registered");
            scene.clear();
            require(world.bodyCount() == 0, "Clearing the scene left a registered body");
        }
        require(world.bodyCount() == 0, "Scene destruction left a registered body");

        std::cout << "physics_body_component_test passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "physics_body_component_test failed: " << error.what() << '\n';
        return 1;
    }
}
