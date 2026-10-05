#include "TestSupport.h"
#include <GL/glew.h>
#include "platform/Window.h"
#include "resources/PrimitiveResources.h"
#include "resources/ResourceManager.h"
#include "graphics/resources/Material.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
    void write(const std::filesystem::path &path, const std::string &text)
    {
        std::ofstream file(path); file << text;
        require(bool(file), "Could not write primitive test fixture");
    }

    std::string read(const std::filesystem::path &path)
    {
        std::ifstream file(path);
        require(bool(file), "Could not read primitive test fixture");
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    std::string fixture(const std::string &type, const std::string &radius, const std::string &segments)
    {
        return "INFINITE_SCENE 4\nOBJECT_COUNT 1\nOBJECT 1 -1 1 \"bad\"\n"
            "POSITION 0 0 0\nROTATION_QUAT 0 0 0 1\nSCALE 1 1 1\nSORT_ORIGIN 0 0 0\n"
            "MESH \"NONE\"\nMATERIAL \"NONE\"\nPRIMITIVE " + type +
            "\nSIZE 1 1 1\nRADIUS " + radius + "\nHEIGHT 1\nRADIAL_SEGMENTS " + segments +
            "\nLATITUDE_SEGMENTS 16\nCOLOR 0.2 0.4 0.6 1\nEND_OBJECT\nEND_SCENE\n";
    }
}

int main(int argc, char **argv)
{
    try
    {
        require(argc == 2, "Expected primitive test output directory");
        const auto directory = std::filesystem::absolute(std::filesystem::u8path(argv[1]) / std::filesystem::u8path(u8"几何存档"));
        std::filesystem::create_directories(directory);
        const auto path = directory / "primitives.scene", invalidPath = directory / "invalid.scene";
        const auto assetMesh = std::filesystem::absolute("tests/fixtures/assets/quad.obj");
        const auto assetMaterial = std::filesystem::absolute("tests/fixtures/assets/test.material");
        Window window(128,128,"Primitive serialization",false);
        PrimitiveResources primitives;
        ResourceManager files;
        Scene source(primitives, files);
        std::vector<const GameObject *> originals;
        originals.push_back(&source.createObject("root"));
        for (auto type : {PrimitiveType::Cube, PrimitiveType::Plane, PrimitiveType::Disk,
            PrimitiveType::Sphere, PrimitiveType::Cylinder, PrimitiveType::Cone})
        {
            PrimitiveDescription d; d.type = type; d.size = {2,3,4}; d.radius = .7f; d.height = 1.8f;
            d.radialSegments = 12; d.latitudeSegments = 8;
            PrimitiveOptions options; options.color = {.2f, .4f, .6f, type == PrimitiveType::Sphere ? .5f : 1.0f};
            auto &object = source.createPrimitive(d, options);
            object.transform.position = {float(originals.size()), 1, -2};
            object.transform.setEulerAngles({.1f,.2f,.3f});
            object.transform.scale = {-1,2,1};
            object.transform.setParent(&source.findObject(1)->transform);
            originals.push_back(&object);
        }
        source.findObject(2)->setName(""); // 空名字存取后不能被默认形状名替代。
        source.findObject(3)->setActive(false);
        CylinderOptions options; options.materialPath = assetMaterial;
        originals.push_back(&source.createCylinder(options));
        auto &fileObject = source.createObject("OBJ");
        fileObject.renderable().setFromFiles(files, assetMesh, assetMaterial);
        originals.push_back(&fileObject);
        SceneSerializer::save(source, path);
        require(read(path).find("INFINITE_SCENE 4") == 0, "Scene format version not updated");

        PrimitiveResources restoredPrimitives;
        ResourceManager restoredFiles;
        Scene restored(restoredPrimitives, restoredFiles);
        // 从不同工作目录加载；内置Shader不应查找示例文件，材质路径相对场景文件解析。
        const auto oldDirectory = std::filesystem::current_path();
        std::filesystem::current_path(directory);
        const auto ids = SceneSerializer::load(restored, path, restoredFiles);
        std::filesystem::current_path(oldDirectory);
        require(ids.size() == originals.size(), "Primitive round-trip count changed");
        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            const auto &before = *originals[i], &after = *restored.findObject(ids[i]);
            require(before.name() == after.name() && before.isActive() == after.isActive(), "Primitive name/state changed");
            require(before.transform.position == after.transform.position && before.transform.scale == after.transform.scale,
                "Primitive transform changed");
            const auto &a = before.renderable().primitiveDescription(), &b = after.renderable().primitiveDescription();
            require(bool(a) == bool(b), "Primitive provenance lost");
            if (a)
            {
                require(a->type == b->type && a->size == b->size && a->radius == b->radius && a->height == b->height &&
                    a->radialSegments == b->radialSegments && a->latitudeSegments == b->latitudeSegments, "Primitive parameters changed");
                require(before.material()->baseColor() == after.material()->baseColor() &&
                    before.material()->renderMode() == after.material()->renderMode(), "Primitive material changed");
            }
        }
        require(restored.findObject(ids[1])->transform.parent() == &restored.findObject(ids[0])->transform, "Primitive hierarchy lost");
        require(restored.findObject(ids[7])->material() == restored.findObject(ids[8])->material(), "File material cache not shared");
        auto &extra = restored.createPrimitive(*restored.findObject(ids[1])->renderable().primitiveDescription());
        require(extra.mesh() == restored.findObject(ids[1])->mesh(), "Restored mesh not cached");

        Scene preserved(primitives, files);
        preserved.createObject("keep");
        for (const auto &text : {fixture("Unknown", ".5", "32"), fixture("Cylinder", "-1", "32"),
            fixture("Sphere", ".5", "4294967296"), fixture("Sphere", ".5", "-4294967264"),
            fixture("Cylinder", "nan", "32"), fixture("Cylinder", ".5", "513")})
        {
            write(invalidPath, text);
            expectThrow<std::runtime_error>([&] { SceneSerializer::load(preserved, invalidPath); }, "Invalid primitive accepted");
            require(preserved.objectCount() == 1 && preserved.findObject(1)->name() == "keep", "Failed load changed Scene");
        }
        require(preserved.createObject("next").id() == 2, "Failed load consumed IDs");

        Scene unbound;
        unbound.createObject("keep");
        expectThrow<std::logic_error>([&] { SceneSerializer::load(unbound, path, files); }, "Missing geometry service accepted");
        require(unbound.objectCount() == 1, "Missing service discarded old scene");

        // 原来的文件无路径规则仍成立，不能因为新增几何体功能就静默接受任意内存材质。
        Scene runtime(primitives);
        PrimitiveOptions custom;
        custom.material = primitives.createMaterial(PrimitiveType::Cube, glm::vec4(1));
        runtime.createPrimitive(PrimitiveType::Cube, custom);
        write(invalidPath, "keep existing save");
        expectThrow<std::invalid_argument>([&] { SceneSerializer::save(runtime, invalidPath); }, "Runtime material silently saved");
        require(read(invalidPath) == "keep existing save", "Failed save truncated file");

        // v4只描述旧内置材质；追加PBR参数不能被保存成看似成功却丢掉参数的旧材质。
        Scene changedBuiltin(primitives);
        auto &changedObject = changedBuiltin.createCube();
        const_cast<Material *>(changedObject.material())->setPbrParameters(PbrParameters{});
        expectThrow<std::invalid_argument>([&] { SceneSerializer::save(changedBuiltin, invalidPath); },
            "Builtin material with PBR parameters was silently saved");
        require(read(invalidPath) == "keep existing save", "PBR save rejection damaged previous file");

        Scene rebound(primitives);
        auto &object = rebound.createCube();
        object.setRenderable(*object.mesh(), *object.material());
        require(!object.renderable().primitiveDescription(), "Manual binding retained stale provenance");
        expectThrow<std::invalid_argument>([&] { SceneSerializer::save(rebound, invalidPath); }, "Rebound geometry silently saved");
        object.clearRenderable();
        SceneSerializer::save(rebound, invalidPath);
        SceneSerializer::load(rebound, invalidPath);
        require(!rebound.findObject(2)->renderable().isBound(), "Clear re-created old primitive");
        require(glGetError() == GL_NO_ERROR, "Serialization left GL errors");
        std::cout << "Primitive round-trip, file materials, provenance and rollback passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
