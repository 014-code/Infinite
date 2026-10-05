#include "SceneSerializer.h"

#include "Scene.h"
#include "resources/ResourceManager.h"
#include "serialization/PrimitiveSerialization.h"
#include "serialization/PendingSceneFile.h"

#include <glm/vec4.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    constexpr int sceneFormatVersion = 4;
    constexpr std::size_t maximumObjectCount = 100000;
    constexpr std::size_t maximumObjectNameLength = 4096;

    struct ObjectData
    {
        // 这是纯数据中间结果，解析时不创建GameObject或GPU资源。
        // parentIndex引用文件中的对象顺序（从0开始），不是fileId；-1表示没有父节点。
        std::uint64_t fileId = 0;
        long long parentIndex = -1;
        std::string name;
        bool active = true;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        glm::vec3 sortOrigin{0.0f};
        std::filesystem::path meshPath;
        std::filesystem::path materialPath;
        std::optional<scene_serialization::PrimitiveState> primitive;
    };

    void requireToken(std::istream &stream, const char *expected, const char *context)
    {
        std::string actual;
        if (!(stream >> actual) || actual != expected)
        {
            throw std::runtime_error(std::string("Invalid scene file: expected ") + expected +
                " while reading " + context);
        }
    }

    void requireFinite(const glm::vec3 &value, const char *field)
    {
        if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
        {
            throw std::runtime_error(std::string("Invalid scene file: non-finite ") + field);
        }
    }

    void readVector(std::istream &stream, glm::vec3 &value, const char *field)
    {
        if (!(stream >> value.x >> value.y >> value.z))
        {
            throw std::runtime_error(std::string("Invalid scene file: malformed ") + field);
        }
        requireFinite(value, field);
    }

    std::filesystem::path resolveAssetPath(
        const std::filesystem::path &scenePath,
        const std::string &value)
    {
        // 场景中的相对资源路径相对.scene文件，而不是相对当前工作目录。
        // NONE表示空Renderable，空字符串则视为格式错误，避免两种含义混淆。
        if (value == "NONE") { return {}; }
        if (value.empty()) { throw std::runtime_error("Scene asset path must not be empty"); }
        const auto assetPath = std::filesystem::u8path(value);
        if (assetPath.is_absolute()) { return assetPath.lexically_normal(); }
        return (scenePath.parent_path() / assetPath).lexically_normal();
    }

    std::filesystem::path relativeAssetPath(
        const std::filesystem::path &scenePath,
        const std::filesystem::path &assetPath)
    {
        // 保存时尽量写相对路径，保证整个示例目录移动后仍能按同样的目录结构加载。
        std::error_code error;
        const auto relative = std::filesystem::relative(
            std::filesystem::absolute(assetPath),
            std::filesystem::absolute(scenePath).parent_path(), error);
        if (error || relative.empty())
        {
            throw std::runtime_error("Cannot make asset path relative to scene: " + assetPath.string());
        }
        return relative.lexically_normal();
    }

    std::vector<ObjectData> readSceneFile(const std::filesystem::path &path)
    {
        std::ifstream file(path);
        // 固定用小数点解析浮点数，避免系统区域设置改变同一个场景文件的含义。
        file.imbue(std::locale::classic());
        if (!file)
        {
            throw std::runtime_error("Failed to open scene file: " + path.string());
        }

        requireToken(file, "INFINITE_SCENE", "file header");
        int version = 0;
        if (!(file >> version) || version < 1 || version > sceneFormatVersion)
        {
            throw std::runtime_error("Unsupported scene file version");
        }

        requireToken(file, "OBJECT_COUNT", "object count");
        std::size_t objectCount = 0;
        if (!(file >> objectCount) || objectCount > maximumObjectCount)
        {
            throw std::runtime_error("Invalid scene file: object count is too large or malformed");
        }

        std::vector<ObjectData> objects;
        objects.reserve(objectCount);
        std::unordered_set<std::uint64_t> fileIds;
        for (std::size_t index = 0; index < objectCount; ++index)
        {
            requireToken(file, "OBJECT", "object header");
            ObjectData object;
            int active = 0;
            if (!(file >> object.fileId >> object.parentIndex >> active >> std::quoted(object.name)))
            {
                throw std::runtime_error("Invalid scene file: malformed object header");
            }
            if (object.fileId == 0 || !fileIds.insert(object.fileId).second)
            {
                throw std::runtime_error("Invalid scene file: duplicate or zero object ID");
            }
            if (active != 0 && active != 1)
            {
                throw std::runtime_error("Invalid scene file: active flag must be 0 or 1");
            }
            if (object.name.size() > maximumObjectNameLength)
            {
                throw std::runtime_error("Invalid scene file: object name is too long");
            }
            object.active = active != 0;

            requireToken(file, "POSITION", "object position");
            readVector(file, object.position, "position");
            if (version == 1)
            {
                glm::vec3 eulerAngles;
                requireToken(file, "ROTATION", "legacy object rotation");
                readVector(file, eulerAngles, "legacy rotation");
                Transform legacyTransform;
                legacyTransform.setEulerAngles(eulerAngles);
                object.rotation = legacyTransform.rotation();
            }
            else
            {
                requireToken(file, "ROTATION_QUAT", "object rotation quaternion");
                glm::vec4 components;
                if (!(file >> components.x >> components.y >> components.z >> components.w))
                {
                    throw std::runtime_error("Invalid scene file: malformed rotation quaternion");
                }
                if (!std::isfinite(components.x) || !std::isfinite(components.y) ||
                    !std::isfinite(components.z) || !std::isfinite(components.w))
                {
                    throw std::runtime_error("Invalid scene file: non-finite rotation quaternion");
                }
                // 文件按x/y/z/w排列，GLM构造按w/x/y/z接收，必须显式换序。
                object.rotation = glm::quat(components.w, components.x, components.y, components.z);
                // 复用Transform的安全归一化，避免大数平方溢出或小数平方下溢。
                Transform normalized;
                try
                {
                    normalized.setRotation(object.rotation);
                }
                catch (const std::invalid_argument &)
                {
                    throw std::runtime_error("Invalid scene file: invalid rotation quaternion");
                }
                object.rotation = normalized.rotation();
            }

            requireToken(file, "SCALE", "scale");
            readVector(file, object.scale, "scale");
            requireToken(file, "SORT_ORIGIN", "sort origin");
            readVector(file, object.sortOrigin, "sort origin");
            if (version >= 3)
            {
                std::string meshPath;
                std::string materialPath;
                requireToken(file, "MESH", "mesh asset reference");
                if (!(file >> std::quoted(meshPath)))
                {
                    throw std::runtime_error("Invalid scene file: malformed mesh asset path");
                }
                requireToken(file, "MATERIAL", "material asset reference");
                if (!(file >> std::quoted(materialPath)))
                {
                    throw std::runtime_error("Invalid scene file: malformed material asset path");
                }
                object.meshPath = resolveAssetPath(path, meshPath);
                object.materialPath = resolveAssetPath(path, materialPath);
            }
            if (version >= 4)
            {
                object.primitive = scene_serialization::readPrimitive(file);
                if (object.primitive && !object.meshPath.empty())
                {
                    throw std::runtime_error("Scene object cannot contain both file Mesh and primitive geometry");
                }
            }
            // 旧格式及文件网格仍要求Mesh/Material成对；基础几何允许单独引用材质文件。
            if (!object.primitive && object.meshPath.empty() != object.materialPath.empty())
            {
                throw std::runtime_error("Invalid scene file: Mesh and Material references must appear together");
            }
            requireToken(file, "END_OBJECT", "object end");
            objects.push_back(std::move(object));
        }

        requireToken(file, "END_SCENE", "scene end");
        std::string unexpected;
        if (file >> unexpected)
        {
            throw std::runtime_error("Invalid scene file: unexpected data after END_SCENE");
        }

        for (std::size_t index = 0; index < objects.size(); ++index)
        {
            const long long parent = objects[index].parentIndex;
            if (parent < -1 || parent >= static_cast<long long>(objects.size()))
            {
                throw std::runtime_error("Invalid scene file: parent index is out of range");
            }
        }
        // 颜色标记：0=未访问，1=正在沿父链访问，2=已完成；遇到1说明形成环。
        std::vector<unsigned char> colors(objects.size(), 0);
        const auto visit = [&](const auto &self, std::size_t index) -> void
        {
            if (colors[index] == 1)
            {
                throw std::runtime_error("Invalid scene file: parent relationship contains a cycle");
            }
            if (colors[index] == 2) { return; }
            colors[index] = 1;
            const long long parent = objects[index].parentIndex;
            if (parent >= 0) { self(self, static_cast<std::size_t>(parent)); }
            colors[index] = 2;
        };
        for (std::size_t index = 0; index < objects.size(); ++index) { visit(visit, index); }
        return objects;
    }
}

void SceneSerializer::save(const Scene &scene, const std::filesystem::path &path)
{
    if (path.empty()) { throw std::invalid_argument("Scene path must not be empty"); }
    // 先在内存中完成校验和格式化，避免无文件来源的资源等逻辑错误截断已有存档。
    // 内容准备完成后通过独占临时文件提交，不直接截断旧档；不承诺断电持久化。
    std::ostringstream file;
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file.imbue(std::locale::classic());

    // 文件中的OBJECT ID只用于校验和人工阅读；加载时不会复用它，运行时ID继续由Scene递增分配。
    file << "INFINITE_SCENE " << sceneFormatVersion << '\n';
    file << "OBJECT_COUNT " << scene.objects_.size() << '\n';
    for (std::size_t index = 0; index < scene.objects_.size(); ++index)
    {
        const GameObject &object = *scene.objects_[index];
        requireFinite(object.transform.position, "position");
        requireFinite(object.transform.scale, "scale");
        requireFinite(object.sortOrigin(), "sort origin");
        long long parentIndex = -1;
        if (const Transform *parent = object.transform.parent())
        {
            std::size_t candidateIndex = 0;
            for (; candidateIndex < scene.objects_.size(); ++candidateIndex)
            {
                if (&scene.objects_[candidateIndex]->transform == parent) { break; }
            }
            if (candidateIndex == scene.objects_.size())
            {
                throw std::invalid_argument("Cannot save a Scene with an external Transform parent");
            }
            parentIndex = static_cast<long long>(candidateIndex);
        }

        file << "OBJECT " << object.id() << ' ' << parentIndex << ' ' << (object.isActive() ? 1 : 0)
             << ' ' << std::quoted(object.name()) << '\n';
        // 足够的有效数字保证float写成文本再读回时不会因默认精度而丢失变换信息。
        file << "POSITION " << std::setprecision(std::numeric_limits<float>::max_digits10)
             << object.transform.position.x << ' ' << object.transform.position.y << ' '
             << object.transform.position.z << '\n';
        const glm::quat &rotation = object.transform.rotation();
        file << "ROTATION_QUAT " << rotation.x << ' ' << rotation.y << ' '
             << rotation.z << ' ' << rotation.w << '\n';
        file << "SCALE " << object.transform.scale.x << ' ' << object.transform.scale.y << ' '
             << object.transform.scale.z << '\n';
        file << "SORT_ORIGIN " << object.sortOrigin().x << ' ' << object.sortOrigin().y << ' '
             << object.sortOrigin().z << '\n';

        const Renderable &renderable = object.renderable();
        if (renderable.isBound() && renderable.meshPath().empty() && !renderable.primitiveDescription())
        {
            throw std::invalid_argument("Cannot save a runtime-only Renderable without asset paths");
        }
        if (!renderable.primitiveDescription() && renderable.meshPath().empty() != renderable.materialPath().empty())
        {
            throw std::invalid_argument("Renderable must have both Mesh and Material asset paths");
        }
        if (renderable.primitiveDescription())
        {
            file << "MESH \"NONE\"\nMATERIAL " << std::quoted(renderable.materialPath().empty() ?
                std::string("NONE") : relativeAssetPath(path, renderable.materialPath()).u8string()) << '\n';
        }
        else if (renderable.meshPath().empty())
        {
            file << "MESH \"NONE\"\nMATERIAL \"NONE\"\n";
        }
        else
        {
            file << "MESH " << std::quoted(relativeAssetPath(path, renderable.meshPath()).u8string()) << '\n';
            file << "MATERIAL " << std::quoted(relativeAssetPath(path, renderable.materialPath()).u8string()) << '\n';
        }
        scene_serialization::writePrimitive(file, renderable);
        file << "END_OBJECT\n";
    }
    file << "END_SCENE\n";
    scene_serialization::PendingSceneFile output(path);
    output.write(file.str());
    output.commit();
}

std::vector<ObjectId> SceneSerializer::load(Scene &scene, const std::filesystem::path &path)
{
    return loadImpl(scene, path, nullptr);
}

std::vector<ObjectId> SceneSerializer::load(
    Scene &scene, const std::filesystem::path &path, ResourceManager &resources)
{
    return loadImpl(scene, path, &resources);
}

std::vector<ObjectId> SceneSerializer::loadImpl(
    Scene &scene, const std::filesystem::path &path, ResourceManager *resources)
{
    if (scene.updating_)
    {
        throw std::logic_error("Cannot load a Scene during Scene::update");
    }
    const std::vector<ObjectData> objects = readSceneFile(path);

    // 保留单调递增的运行时ID；文件ID用于校验文件，父子关系按文件中的索引恢复。
    Scene loadedScene;
    // 临时场景借用同一几何体服务；它只共享资源，不共享或修改目标场景中的物体。
    loadedScene.primitiveResources_ = scene.primitiveResources_;
    loadedScene.fileResources_ = resources != nullptr ? resources : scene.fileResources_;
    loadedScene.nextId_ = scene.nextId_;
    std::vector<GameObject *> loaded;
    loaded.reserve(objects.size());
    for (const ObjectData &data : objects)
    {
        PrimitiveOptions options;
        options.name = data.name;
        if (data.primitive)
        {
            options.color = data.primitive->color;
            options.materialPath = data.materialPath;
        }
        GameObject &object = data.primitive ?
            loadedScene.createPrimitive(data.primitive->geometry, options) : loadedScene.createObject(data.name);
        // 空名字也是合法存档内容，不要让创建接口的默认名称覆盖它。
        if (data.primitive) { object.setName(data.name); }
        object.setActive(data.active);
        object.transform.position = data.position;
        object.transform.setRotation(data.rotation);
        object.transform.scale = data.scale;
        object.setSortOrigin(data.sortOrigin);
        if (!data.meshPath.empty())
        {
            if (resources == nullptr)
            {
                throw std::invalid_argument("Scene contains asset references but no ResourceManager was provided");
            }
            object.renderable().setFromFiles(*resources, data.meshPath, data.materialPath);
        }
        loaded.push_back(&object);
    }
    // 先创建全部对象再连父子关系，文件中的父节点可以出现在子节点之后。
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        const long long parent = objects[index].parentIndex;
        loaded[index]->transform.setParent(parent < 0 ? nullptr :
            &loaded[static_cast<std::size_t>(parent)]->transform);
    }

    std::vector<ObjectId> ids;
    ids.reserve(loaded.size());
    for (const GameObject *object : loaded) { ids.push_back(object->id()); }

    // 只有资源、对象和父子关系全部成功后才替换旧场景；失败路径不会触碰旧场景。
    // 由于loaded中的GameObject由loadedScene独占，移动objects_后父子Transform地址仍保持有效。
    scene.objects_ = std::move(loadedScene.objects_);
    scene.nextId_ = loadedScene.nextId_;
    return ids;
}
