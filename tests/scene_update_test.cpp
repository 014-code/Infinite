#include "TestSupport.h"
#include "scene/Scene.h"

#include <iostream>
#include <limits>

int main()
{
    try
    {
        Scene scene;
        auto &first = scene.createObject("first");
        auto &second = scene.createObject("second");
        require(first.script().owner() == &first, "Script component owner is wrong");
        require(first.renderable().owner() == &first, "Renderable component owner is wrong");
        require(first.physicsBody().owner() == &first, "Physics component owner is wrong");
        require(first.characterBody().owner() == &first, "Character component owner is wrong");
        std::vector<ObjectId> order;
        // 直接通过组件注册第一份逻辑，确认行为已经由ScriptComponent持有。
        first.script().setUpdateCallback([&](GameObject &object, float dt)
        {
            object.transform.position.x += dt;
            order.push_back(object.id());
        });
        require(first.script().hasUpdateCallback(), "Script component did not store callback");
        // 第二个对象继续使用兼容入口，保证旧示例代码无需立即全部迁移。
        second.setUpdateCallback([&](GameObject &object, float) { order.push_back(object.id()); });
        scene.update(0.25f);
        require(order == std::vector<ObjectId>{first.id(), second.id()}, "Wrong update order");
        require(first.transform.position.x == 0.25f, "deltaTime not forwarded");
        second.setActive(false);
        order.clear();
        scene.update(0.0f);
        require(order == std::vector<ObjectId>{first.id()}, "Inactive object updated");
        expectThrow<std::invalid_argument>([&] { scene.update(-1); }, "Negative dt accepted");
        expectThrow<std::invalid_argument>([&] { scene.update(std::numeric_limits<float>::quiet_NaN()); }, "NaN dt accepted");
        first.setUpdateCallback([&](GameObject &object, float)
        {
            expectThrow<std::logic_error>([&] { scene.update(0); }, "Recursive update accepted");
            expectThrow<std::logic_error>([&] { scene.createObject("invalid"); }, "Creation accepted during update");
            expectThrow<std::logic_error>([&] { scene.removeObject(object.id()); }, "Removal accepted during update");
            expectThrow<std::logic_error>([&] { scene.clear(); }, "Clear accepted during update");
            object.setUpdateCallback({});
        });
        scene.update(0);
        // mutable捕获属于回调自身，连续帧必须调用同一个函数对象而不是临时副本。
        first.setUpdateCallback([count = 0](GameObject &object, float) mutable
        {
            object.transform.position.y = static_cast<float>(++count);
        });
        scene.update(0);
        scene.update(0);
        require(first.transform.position.y == 2.0f, "Mutable callback state lost across frames");
        first.setUpdateCallback([](GameObject &object, float)
        {
            object.setUpdateCallback([](GameObject &next, float)
            {
                next.transform.position.z += 1.0f;
            });
        });
        scene.update(0);
        require(first.transform.position.z == 0.0f, "Replacement ran in the same update");
        scene.update(0);
        require(first.transform.position.z == 1.0f, "Replacement did not run next frame");
        first.setUpdateCallback([](GameObject &, float) { throw std::runtime_error("user error"); });
        expectThrow<std::runtime_error>([&] { scene.update(0); }, "User exception swallowed");
        // 回调失败不应把Scene永久锁在更新状态。
        scene.clear();
        scene.createObject("after error");
        scene.update(0);
        std::cout << "Scene update passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
