#include "TestSupport.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"

#include <glm/vec3.hpp>

#include <filesystem>
#include <fstream>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    void requireNear(float actual, float expected, const char *message)
    {
        require(std::abs(actual - expected) < 0.0001f, message);
    }

    void writeText(const std::filesystem::path &path, const char *text)
    {
        std::ofstream file(path);
        if (!file) { throw std::runtime_error("Could not create test scene file"); }
        file << text;
    }
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected scene test output directory");
        const std::filesystem::path directory = argv[1];
        std::filesystem::create_directories(directory);
        const auto scenePath = directory / "hierarchy.scene";

        Scene source;
        auto &root = source.createObject("根节点 \"Root\"");
        auto &child = source.createObject("child");
        auto &grandchild = source.createObject("grandchild");
        root.setActive(false);
        root.transform.position = {10.0f, 1.0f, -2.0f};
        root.transform.setEulerAngles({0.1f, 0.2f, 0.3f});
        root.transform.scale = {2.0f, 3.0f, 4.0f};
        child.transform.position = {1.0f, 2.0f, 3.0f};
        child.setSortOrigin({0.5f, -0.5f, 2.0f});
        grandchild.transform.position.x = -4.0f;
        child.transform.setParent(&root.transform);
        grandchild.transform.setParent(&child.transform);

        SceneSerializer::save(source, scenePath);

        Scene restored;
        SceneSerializer::load(restored, scenePath);
        require(restored.objectCount() == 3, "Scene object count was not restored");
        const GameObject *restoredRoot = restored.findObject(1);
        const GameObject *restoredChild = restored.findObject(2);
        const GameObject *restoredGrandchild = restored.findObject(3);
        require(restoredRoot != nullptr && restoredChild != nullptr && restoredGrandchild != nullptr,
            "Loaded Scene did not create expected local IDs");
        require(restoredRoot->name() == "根节点 \"Root\"" && !restoredRoot->isActive(),
            "Object name or active state was not restored");
        requireNear(restoredRoot->transform.position.x, 10.0f, "Position was not restored");
        requireNear(restoredRoot->transform.eulerAngles().y, 0.2f, "Rotation was not restored");
        requireNear(restoredRoot->transform.scale.z, 4.0f, "Scale was not restored");
        require(restoredChild->transform.parent() == &restoredRoot->transform &&
            restoredGrandchild->transform.parent() == &restoredChild->transform,
            "Parent hierarchy was not restored");
        requireNear(restoredChild->sortOrigin().z, 2.0f, "Sort origin was not restored");

        // 旧版文件仍使用三个欧拉角。加载后对比完整旋转矩阵，确保复合角度
        // 的先后顺序与旧Transform一致，而不只是验证某一个欧拉角分量。
        const auto legacyPath = directory / "legacy.scene";
        writeText(legacyPath,
            "INFINITE_SCENE 1\nOBJECT_COUNT 1\n"
            "OBJECT 7 -1 1 \"legacy\"\n"
            "POSITION 0 0 0\nROTATION 0.2 0.4 -0.3\nSCALE 1 1 1\n"
            "SORT_ORIGIN 0 0 0\nEND_OBJECT\nEND_SCENE\n");
        Scene legacy;
        SceneSerializer::load(legacy, legacyPath);
        require(legacy.objectCount() == 1, "Legacy scene object was not loaded");
        requireNear(legacy.findObject(1)->transform.eulerAngles().x, 0.2f,
            "Legacy scene rotation X changed");
        requireNear(legacy.findObject(1)->transform.eulerAngles().y, 0.4f,
            "Legacy scene rotation Y changed");
        requireNear(legacy.findObject(1)->transform.eulerAngles().z, -0.3f,
            "Legacy scene rotation Z changed");

        // 版本2没有资源字段；极大但有限的四元数仍应安全归一化为单位旋转。
        writeText(legacyPath,
            "INFINITE_SCENE 2\nOBJECT_COUNT 1\nOBJECT 1 -1 1 \"v2\"\n"
            "POSITION 0 0 0\nROTATION_QUAT 0 0 0 1e30\nSCALE 1 1 1\n"
            "SORT_ORIGIN 0 0 0\nEND_OBJECT\nEND_SCENE\n");
        const auto versionTwoIds = SceneSerializer::load(legacy, legacyPath);
        requireNear(legacy.findObject(versionTwoIds[0])->transform.rotation().w, 1.0f,
            "Version 2 quaternion normalization failed");

        // 解析失败必须发生在Scene清空之前，已有对象应保持原状。
        Scene preserved;
        preserved.createObject("keep me");
        const auto invalidPath = directory / "invalid.scene";
        writeText(invalidPath,
            "INFINITE_SCENE 999\nOBJECT_COUNT 0\nEND_SCENE\n");
        expectThrow<std::runtime_error>([&] { SceneSerializer::load(preserved, invalidPath); },
            "Unsupported scene version was accepted");
        require(preserved.objectCount() == 1 && preserved.findObject(1) != nullptr,
            "Failed load modified the existing Scene");

        writeText(invalidPath,
            "INFINITE_SCENE 1\nOBJECT_COUNT 1\n"
            "OBJECT 1 4 1 \"bad parent\"\n"
            "POSITION 0 0 0\nROTATION 0 0 0\nSCALE 1 1 1\n"
            "SORT_ORIGIN 0 0 0\nEND_OBJECT\nEND_SCENE\n");
        expectThrow<std::runtime_error>([&] { SceneSerializer::load(preserved, invalidPath); },
            "Out-of-range parent index was accepted");
        require(preserved.objectCount() == 1, "Invalid hierarchy load changed the Scene");

        std::cout << "Scene serialization passed: versioning, transforms, hierarchy and validation\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Scene serialization failed: " << error.what() << '\n';
        return 1;
    }
}
