#include "TestSupport.h"

#include "platform/Window.h"
#include "resources/ResourceManager.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    void writeText(const std::filesystem::path &path, const std::string &text)
    {
        std::ofstream file(path);
        if (!file)
        {
            throw std::runtime_error("Could not create scene resource test file");
        }
        file << text;
    }
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected scene resource test output directory");
        const std::filesystem::path directory = argv[1];
        std::filesystem::create_directories(directory);

        // Mesh、Material、Shader和Texture都可能创建OpenGL对象，测试先创建隐藏上下文。
        Window window(128, 128, "Scene Resource Test", false);
        ResourceManager resources;
        const auto meshPath = std::filesystem::absolute("tests/fixtures/assets/quad.obj");
        const auto materialPath = std::filesystem::absolute("tests/fixtures/assets/test.material");
        const auto scenePath = directory / "asset_scene.scene";

        Scene source;
        auto &root = source.createObject("resource root");
        root.renderable().setFromFiles(resources, meshPath, materialPath);
        auto &child = source.createObject("resource child");
        child.renderable().setFromFiles(resources, meshPath, materialPath);
        child.transform.setParent(&root.transform);
        SceneSerializer::save(source, scenePath);

        // 使用另一个ResourceManager加载，确保场景文件本身保存了完整的资源引用。
        ResourceManager restoredResources;
        Scene restored;
        const std::vector<ObjectId> ids =
            SceneSerializer::load(restored, scenePath, restoredResources);
        require(ids.size() == 2 && restored.objectCount() == 2,
            "Scene resource references did not restore object count");
        const GameObject *restoredRoot = restored.findObject(ids[0]);
        const GameObject *restoredChild = restored.findObject(ids[1]);
        require(restoredRoot != nullptr && restoredChild != nullptr,
            "Scene resource load did not return valid object IDs");
        require(restoredRoot->renderable().isBound() && restoredChild->renderable().isBound(),
            "Scene resource load did not bind Mesh and Material");
        require(restoredRoot->renderable().mesh() == restoredChild->renderable().mesh() &&
            restoredRoot->renderable().material() == restoredChild->renderable().material(),
            "Scene objects did not share ResourceManager cache entries");
        require(restoredChild->transform.parent() == &restoredRoot->transform,
            "Scene resource load did not restore parent relationship");
        require(restoredResources.meshCount() == 1 && restoredResources.materialCount() == 1 &&
            restoredResources.shaderCount() == 1 && restoredResources.textureCount() == 1,
            "Scene resource load created duplicate cached resources");

        // 没有ResourceManager时不能静默丢掉资源配置；旧场景应保持不变。
        Scene preserved;
        preserved.createObject("keep me");
        expectThrow<std::invalid_argument>([&]
        {
            SceneSerializer::load(preserved, scenePath);
        }, "Asset scene loaded without ResourceManager");
        require(preserved.objectCount() == 1 && preserved.findObject(1) != nullptr,
            "Missing ResourceManager changed the existing Scene");

        // 资源文件不存在时，加载在临时Scene中失败，不能清空调用者当前场景。
        const auto missingPath = directory / "missing_asset.scene";
        writeText(missingPath,
            "INFINITE_SCENE 3\nOBJECT_COUNT 1\n"
            "OBJECT 7 -1 1 \"missing\"\n"
            "POSITION 0 0 0\nROTATION_QUAT 0 0 0 1\nSCALE 1 1 1\n"
            "SORT_ORIGIN 0 0 0\nMESH \"missing.obj\"\n"
            "MATERIAL \"missing.material\"\nEND_OBJECT\nEND_SCENE\n");
        expectThrow<std::runtime_error>([&]
        {
            SceneSerializer::load(preserved, missingPath, restoredResources);
        }, "Missing scene assets were accepted");
        require(preserved.objectCount() == 1 && preserved.findObject(1) != nullptr,
            "Missing scene assets changed the existing Scene");

        // 第二个对象的网格已成功加载、材质才失败，仍不能提交第一个对象或消耗旧ID。
        std::ostringstream lateFailure;
        lateFailure << "INFINITE_SCENE 3\nOBJECT_COUNT 2\n"
            "OBJECT 1 -1 1 \"valid\"\nPOSITION 0 0 0\nROTATION_QUAT 0 0 0 1\n"
            "SCALE 1 1 1\nSORT_ORIGIN 0 0 0\nMESH " << std::quoted(meshPath.u8string())
            << "\nMATERIAL " << std::quoted(materialPath.u8string()) << "\nEND_OBJECT\n"
            "OBJECT 2 0 1 \"broken\"\nPOSITION 0 0 0\nROTATION_QUAT 0 0 0 1\n"
            "SCALE 1 1 1\nSORT_ORIGIN 0 0 0\nMESH " << std::quoted(meshPath.u8string())
            << "\nMATERIAL \"missing.material\"\nEND_OBJECT\nEND_SCENE\n";
        writeText(missingPath, lateFailure.str());
        expectThrow<std::runtime_error>([&]
        {
            SceneSerializer::load(preserved, missingPath, restoredResources);
        }, "Partial resource load was committed");
        require(preserved.objectCount() == 1 && preserved.findObject(1)->name() == "keep me" &&
            preserved.createObject("next").id() == 2, "Failed load changed Scene or ID sequence");

        // 成功替换已有场景后，旧ID不能重新指向新物体。
        const auto replacementIds = SceneSerializer::load(preserved, scenePath, restoredResources);
        require(replacementIds[0] == 3 && preserved.findObject(1) == nullptr &&
            preserved.findObject(2) == nullptr, "Successful load reused stale object IDs");

        // 运行时手工绑定的GPU资源没有可重建的文件来源，保存时必须明确报错。
        Scene runtimeOnly;
        auto &runtimeObject = runtimeOnly.createObject("runtime only");
        auto mesh = restoredResources.loadMesh(meshPath);
        auto material = restoredResources.loadMaterial(materialPath);
        runtimeObject.setRenderable(*mesh, *material);
        const auto runtimePath = directory / "runtime_only.scene";
        writeText(runtimePath, "existing save");
        expectThrow<std::invalid_argument>([&]
        {
            SceneSerializer::save(runtimeOnly, runtimePath);
        }, "Runtime-only Renderable was silently serialized");
        std::ifstream savedFile(runtimePath);
        std::string savedText;
        std::getline(savedFile, savedText);
        require(savedText == "existing save", "Invalid save truncated the existing file");

        std::cout << "Scene resource references passed: loading, caching and failure safety\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Scene resource references failed: " << error.what() << '\n';
        return 1;
    }
}
