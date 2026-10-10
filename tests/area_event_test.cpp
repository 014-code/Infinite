#include "TestSupport.h"
#include "math/Transform.h"
#include "physics/world/PhysicsWorld.h"
#include "scene/components/AreaComponent.h"
#include "scene/GameObject.h"
#include "scene/Scene.h"

#include <iostream>

int main()
{
    try
    {
        PhysicsWorld world;
        const PhysicsBodyId body = world.createStaticBody(
            CollisionShape(SphereShape(0.5f)), {0.0f, 0.0f, 0.0f});
        AreaComponent area;
        area.attach(world, CollisionShape(SphereShape(1.0f)));

        int entered = 0;
        int exited = 0;
        area.setBodyEnteredCallback([&](PhysicsBodyId id)
        {
            require(id == body, "Area entered wrong body");
            ++entered;
        });
        area.setBodyExitedCallback([&](PhysicsBodyId id)
        {
            require(id == body, "Area exited wrong body");
            ++exited;
        });

        Transform inside;
        area.poll(inside);
        require(entered == 1 && exited == 0, "Area enter event was not generated");
        area.poll(inside);
        require(entered == 1, "Area generated duplicate enter event");

        inside.position = {5.0f, 0.0f, 0.0f};
        area.poll(inside);
        require(exited == 1, "Area exit event was not generated");

        // Area查询使用世界空间位姿；父节点和非单位缩放必须显式拒绝，
        // 不能把局部坐标静默当成世界坐标而产生错误的进入事件。
        Transform parent;
        inside.setParent(&parent);
        expectThrow<std::invalid_argument>([&] { area.poll(inside); },
            "Area accepted a Transform with a parent");
        inside.setParent(nullptr);
        inside.scale = glm::vec3(2.0f, 1.0f, 1.0f);
        expectThrow<std::invalid_argument>([&] { area.poll(inside); },
            "Area accepted a non-unit scale");

        // GameObject在绑定阶段也应提前拒绝非法Transform，避免先创建一个不可同步的Area。
        Scene scene;
        GameObject &object = scene.createObject("invalid_area_transform");
        object.transform.scale = glm::vec3(2.0f, 1.0f, 1.0f);
        expectThrow<std::invalid_argument>([&] {
            object.setArea(world, CollisionShape(SphereShape(1.0f))); },
            "GameObject accepted an Area with a non-unit scale");
        require(!object.area().isAttached(), "Invalid Area binding left an attached component");
        area.detach();
        require(!area.isAttached(), "Area detach failed");
        std::cout << "Area events passed\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
