#include "TestSupport.h"
#include "resources/ResourceManager.h"
#include "scene/ModelInstantiator.h"
#include "scene/Scene.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "platform/Window.h"
#include <iostream>
#include <cstdlib>
#include <new>

// 仅此测试进程注入分配失败，验证实例化进行到中途时也会完整回滚。
// 一旦触发失败立刻关闭注入，使异常清理和测试断言本身可以正常分配。
namespace { long allocationsUntilFailure = -1; }
void *operator new(std::size_t bytes)
{
    if (allocationsUntilFailure == 0) { allocationsUntilFailure = -1; throw std::bad_alloc(); }
    if (allocationsUntilFailure > 0) { --allocationsUntilFailure; }
    if (void *pointer = std::malloc(bytes ? bytes : 1)) { return pointer; }
    throw std::bad_alloc();
}
void operator delete(void *pointer) noexcept { std::free(pointer); }
void operator delete(void *pointer, std::size_t) noexcept { std::free(pointer); }

int main()
{
    try
    {
        Window window(64, 64, "Model instance test", false);
        ResourceManager resources;
        const auto vertex = "examples/gltf_model/shaders/preview.vert";
        const auto fragment = "examples/gltf_model/shaders/preview.frag";
        auto model = resources.loadModel("tests/fixtures/gltf/multi.gltf", vertex, fragment);
        require(model == resources.loadModel("tests/fixtures/gltf/./multi.gltf", vertex, fragment) && resources.modelCount() == 1,
            "Model cache was not reused");
        auto alternate = resources.loadModel("tests/fixtures/gltf/multi.gltf",
            "examples/scene_objects/shaders/scene.vert", "examples/scene_objects/shaders/scene.frag");
        require(alternate != model && resources.modelCount() == 2, "Shader not included in model cache key");
        Scene scene;
        // 在连续分配位置逐一失败，覆盖节点字符串、对象分配、父子登记和primitive绑定之前。
        Scene rollback;
        rollback.createObject("existing");
        bool reachedSuccess = false;
        for (long point = 0; point < 128; ++point)
        {
            allocationsUntilFailure = point;
            try
            {
                auto trial = ModelInstantiator::instantiate(rollback, *model, "allocation-failure-fixture");
                allocationsUntilFailure = -1;
                ModelInstantiator::remove(rollback, trial);
                reachedSuccess = true;
                break;
            }
            catch (const std::bad_alloc &)
            {
                allocationsUntilFailure = -1;
                require(rollback.objectCount() == 1 && rollback.findObject(1)->name() == "existing",
                    "Partial model instance survived allocation failure");
            }
        }
        require(reachedSuccess, "Allocation failure sweep never reached success");
        auto first = ModelInstantiator::instantiate(scene, *model, "first");
        auto second = ModelInstantiator::instantiate(scene, *model, "second");
        require(first.objectIds.size() == 5 && scene.objectCount() == 10, "Primitive instance objects missing");
        auto *a = scene.findObject(first.objectIds.back());
        auto *b = scene.findObject(second.objectIds.back());
        require(a->mesh() == b->mesh() && a->material() == b->material(), "Instances did not share resources");
        scene.findObject(first.rootId)->transform.position.x = 2;
        require(a->transform.worldMatrix()[3].x == 2 && b->transform.worldMatrix()[3].x == 0, "Instance transforms were shared");
        require(a->transform.parent() == &scene.findObject(first.nodeIds[1])->transform, "Primitive parent lost");
        expectThrow<std::runtime_error>([&] { resources.loadModel("tests/fixtures/gltf/blend.gltf", vertex, fragment); }, "Failed model cached");
        require(resources.modelCount() == 2, "Failed model changed cache");
        auto &guard = scene.createObject("guard");
        guard.setUpdateCallback([&](GameObject &, float)
        {
            expectThrow<std::logic_error>([&] { ModelInstantiator::instantiate(scene, *model); }, "Update allowed structural mutation");
        });
        scene.update(0);
        require(scene.objectCount() == 11, "Failed instantiation changed scene");
        resources.clear(); model.reset(); alternate.reset();
        // 清缓存和模型指针后，渲染对象仍共享持有Mesh/Material，必须可以继续使用。
        a->material()->use(); a->mesh()->draw();
        require(glGetError() == GL_NO_ERROR, "Resources died with model cache");
        ModelInstantiator::remove(scene, first);
        require(scene.objectCount() == 6 && scene.findObject(second.rootId), "Instance removal affected another instance");
        Scene other;
        expectThrow<std::invalid_argument>([&] { ModelInstantiator::remove(other, second); }, "Wrong Scene accepted");
        ModelInstantiator::remove(scene, second);
        require(scene.objectCount() == 1, "Instance objects leaked");
        std::cout << "Model caching/instances/lifetime passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
