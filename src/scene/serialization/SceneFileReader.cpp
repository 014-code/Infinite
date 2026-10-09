#include "SceneFileReader.h"

#include "math/Transform.h"

#include <glm/vec4.hpp>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace scene_serialization_detail
{
    namespace
    {
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

        std::filesystem::path resolveAssetPath(const std::filesystem::path &scenePath,
            const std::string &value)
        {
            // 场景中的相对资源路径相对.scene文件，而不是相对当前工作目录。
            // NONE表示空Renderable，空字符串则视为格式错误，避免两种含义混淆。
            if (value == "NONE")
            {
                return {};
            }
            if (value.empty())
            {
                throw std::runtime_error("Scene asset path must not be empty");
            }
            const auto assetPath = std::filesystem::u8path(value);
            if (assetPath.is_absolute())
            {
                return assetPath.lexically_normal();
            }
            return (scenePath.parent_path() / assetPath).lexically_normal();
        }

        void validateParentIndices(const ObjectList &objects)
        {
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
                if (colors[index] == 2)
                {
                    return;
                }
                colors[index] = 1;
                const long long parent = objects[index].parentIndex;
                if (parent >= 0)
                {
                    self(self, static_cast<std::size_t>(parent));
                }
                colors[index] = 2;
            };
            for (std::size_t index = 0; index < objects.size(); ++index)
            {
                visit(visit, index);
            }
        }
    }

    ObjectList SceneFileReader::read(const std::filesystem::path &path)
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
        if (!(file >> version) || version < 1 || version > kSceneFormatVersion)
        {
            throw std::runtime_error("Unsupported scene file version");
        }

        requireToken(file, "OBJECT_COUNT", "object count");
        std::size_t objectCount = 0;
        if (!(file >> objectCount) || objectCount > kMaximumObjectCount)
        {
            throw std::runtime_error("Invalid scene file: object count is too large or malformed");
        }

        ObjectList objects;
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
            if (object.name.size() > kMaximumObjectNameLength)
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
        validateParentIndices(objects);
        return objects;
    }
}
