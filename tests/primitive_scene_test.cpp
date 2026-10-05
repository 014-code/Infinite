#include "TestSupport.h"
#include "platform/Window.h"
#include "resources/PrimitiveResources.h"
#include "resources/ResourceManager.h"
#include "scene/Scene.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Material.h"
#include "graphics/resources/Shader.h"

#include <iostream>
#include <limits>

int main()
{
    try
    {
        // 无窗口Scene仍可正常操作空对象；未注入服务时，不能误调用OpenGL。
        Scene cpuScene;
        expectThrow<std::logic_error>([&] { cpuScene.createPrimitive(PrimitiveType::Cube); }, "Missing service accepted");
        require(cpuScene.objectCount() == 0, "Failed creation changed CPU Scene");

        // 已绑定服务但尚未创建窗口时，也必须在调用OpenGL前明确拒绝。
        PrimitiveResources uninitializedResources;
        Scene uninitializedScene(uninitializedResources);
        expectThrow<std::logic_error>([&] { uninitializedScene.createCube(); }, "Missing context accepted");
        require(uninitializedScene.objectCount() == 0, "Missing context left an object");

        Window window(128, 128, "Primitive Scene Test", false);
        PrimitiveResources resources;
        Scene scene(resources);
        auto &first = scene.createPrimitive(PrimitiveType::Cylinder);
        auto &second = scene.createCylinder();
        require(first.mesh() == second.mesh() && resources.meshCount() == 1, "Equivalent meshes not shared");
        require(first.material() != second.material() &&
            &first.material()->shader() == &second.material()->shader(), "Material/Shader ownership incorrect");
        first.transform.position.x = 2;
        require(second.transform.position.x == 0, "Transforms unexpectedly shared");

        CylinderOptions options;
        options.name = "tall cylinder"; options.height = 2; options.color = {0, .5f, 1, 1};
        auto &tall = scene.createCylinder(options);
        require(tall.name() == options.name && tall.mesh() != first.mesh() &&
            tall.mesh()->bounds().maximum.y == 1 && tall.material()->baseColor() == options.color, "Options ignored");
        const auto lastId = tall.id();
        options.radius = -1;
        expectThrow<std::invalid_argument>([&] { scene.createCylinder(options); }, "Invalid radius accepted");
        options.radius = .5f; options.color.x = std::numeric_limits<float>::quiet_NaN();
        expectThrow<std::invalid_argument>([&] { scene.createCylinder(options); }, "Invalid color accepted");
        require(scene.objectCount() == 3 && scene.createObject("next").id() == lastId + 1, "Failure consumed object/ID");

        // 显式用户材质保持共享，不被默认颜色或剔除设置覆盖。
        auto custom = resources.createMaterial(PrimitiveType::Cube, {1, 0, 0, 1});
        options.material = custom;
        auto &customObject = scene.createCylinder(options);
        require(customObject.material() == custom.get(), "Custom material was replaced");
        custom->setBaseColor({0, 1, 0, 1});
        require(customObject.material()->baseColor().g == 1 && first.material()->baseColor().g == .8f, "Material isolation failed");

        ResourceManager fileResources;
        Scene failureScene(resources, fileResources);
        CubeOptions failureOptions;
        failureOptions.material = custom;
        failureOptions.materialPath = "tests/nonexistent-primitive-material.material";
        expectThrow<std::invalid_argument>([&] { failureScene.createCube(failureOptions); }, "Conflicting materials accepted");
        failureOptions.material.reset();
        expectThrow<std::runtime_error>([&] { failureScene.createCube(failureOptions); }, "Missing material file accepted");
        require(failureScene.objectCount() == 0 && failureScene.createObject("next").id() == 1,
            "Material failure consumed object/ID");

        first.setUpdateCallback([&](GameObject &, float)
        {
            expectThrow<std::logic_error>([&] { scene.createCube(); }, "Creation during update allowed");
        });
        scene.update(.01f);
        scene.createCube(); scene.createPlane(); scene.createDisk(); scene.createSphere(); scene.createCone();
        require(glGetError() == GL_NO_ERROR, "OpenGL creation error");

        // 清缓存不使已有物体失效；共享持有者最终在Window析构前释放。
        resources.clear();
        require(resources.meshCount() == 0 && first.mesh()->indexCount() == 384, "Clear invalidated objects");
        first.material()->use(); first.mesh()->draw();
        require(glGetError() == GL_NO_ERROR, "Resource lifetime error");
        expectThrow<std::runtime_error>([]
        {
            Shader::fromSource("not GLSL", "not GLSL", "invalid embedded shader");
        }, "Invalid in-memory shader accepted");
        std::cout << "Primitive Scene resources and failure safety passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
