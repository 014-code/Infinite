#include "TestSupport.h"
#include "scene/Scene.h"
#include "graphics/camera/Camera.h"
#include "graphics/rendering/Renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

int main()
{
    try
    {
        static_assert(!std::is_copy_constructible_v<GameObject> && !std::is_move_constructible_v<GameObject>);
        static_assert(!std::is_copy_constructible_v<Scene> && !std::is_move_constructible_v<Scene>);
        // 完全不创建Window，验证对象管理和排序不依赖GPU上下文。
        Scene scene;
        auto &first = scene.createObject("same name");
        const ObjectId firstId = first.id();
        auto *firstAddress = &first;
        require(firstId != 0 && first.isActive() && first.mesh() == nullptr && first.material() == nullptr,
            "Wrong initial object state");
        first.transform.position.x = 2.0f;
        auto &second = scene.createObject("same name");
        const ObjectId secondId = second.id();
        require(second.id() != firstId && second.transform.position.x == 0.0f,
            "IDs or transforms are not independent");
        for (int index = 0; index < 1024; ++index) { scene.createObject("extra"); }
        require(scene.findObject(firstId) == firstAddress && first.transform.position.x == 2.0f,
            "Growing scene invalidated object address");
        const Scene &constantScene = scene;
        require(constantScene.findObject(firstId) == firstAddress && scene.findObject(0) == nullptr,
            "Const lookup or invalid lookup failed");
        first.setName("renamed");
        first.setActive(false);
        first.setSortOrigin(glm::vec3(1.0f, 2.0f, 3.0f));
        require(first.name() == "renamed" && !first.isActive() && first.sortOrigin().z == 3.0f,
            "Object setters failed");
        require(scene.removeObject(secondId) && !scene.removeObject(secondId) &&
            scene.findObject(secondId) == nullptr && scene.findObject(firstId) == firstAddress,
            "Removing object broke lookup or another object");
        const auto lastId = scene.createObject("last").id();
        scene.clear();
        require(scene.objectCount() == 0 && scene.findObject(firstId) == nullptr, "Clear did not remove objects");
        require(scene.createObject("after clear").id() > lastId, "Clear reused stale IDs");
        // 空物体不会形成绘制项，因此这里即使没有上下文也不应执行任何GL调用。
        Renderer renderer;
        Camera camera;
        scene.render(renderer, camera, 1.0f);
        scene.clear();
        scene.render(renderer, camera, 1.0f);

        Transform nearTransform, farTransform;
        nearTransform.position.z = 0.5f;
        farTransform.position.z = -0.5f;
        std::vector<RenderItem> items = {{nullptr, nullptr, &nearTransform}, {nullptr, nullptr, &farTransform}};
        sortTransparentBackToFront(items, camera.viewMatrix());
        require(items.front().transform == &farTransform, "Incorrect view-space order");
        const auto reverseView = glm::lookAt(glm::vec3(0, 0, -3), glm::vec3(0), glm::vec3(0, 1, 0));
        sortTransparentBackToFront(items, reverseView);
        require(items.front().transform == &nearTransform, "Sort ignored camera direction");
        // 修改局部参考点也必须改变排序；不是只看Transform.position.z。
        items = {{nullptr, nullptr, &nearTransform, glm::vec3(0, 0, -2)}, {nullptr, nullptr, &farTransform}};
        sortTransparentBackToFront(items, camera.viewMatrix());
        require(items.front().transform == &nearTransform, "Sort ignored local origin");
        farTransform.position.z = nearTransform.position.z;
        items = {{nullptr, nullptr, &nearTransform}, {nullptr, nullptr, &farTransform}};
        sortTransparentBackToFront(items, camera.viewMatrix());
        require(items.front().transform == &nearTransform, "Equal-depth sorting was not stable");
        nearTransform.position.z = std::numeric_limits<float>::quiet_NaN();
        bool rejected = false;
        try { sortTransparentBackToFront(items, camera.viewMatrix()); }
        catch (const std::invalid_argument &) { rejected = true; }
        require(rejected, "NaN depth was accepted by sorter");
        std::cout << "Scene management passed: IDs, lifetime rules, stable addresses, transforms and sorting" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Scene management failed: " << exception.what() << std::endl;
        return 1;
    }
}
