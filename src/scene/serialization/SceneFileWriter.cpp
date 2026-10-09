#include "SceneFileWriter.h"

#include "scene/Scene.h"
#include "scene/serialization/SceneData.h"
#include "scene/serialization/PendingSceneFile.h"
#include "scene/serialization/PrimitiveSerialization.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace scene_serialization_detail
{
    namespace
    {
        void requireFinite(const glm::vec3 &value, const char *field)
        {
            if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
            {
                throw std::runtime_error(std::string("Invalid scene file: non-finite ") + field);
            }
        }

        std::filesystem::path relativeAssetPath(const std::filesystem::path &scenePath,
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
    }

    void SceneFileWriter::write(const Scene &scene, const std::filesystem::path &path)
    {
        if (path.empty())
        {
            throw std::invalid_argument("Scene path must not be empty");
        }
        // 先在内存中完成校验和格式化，避免无文件来源的资源等逻辑错误截断已有存档。
        std::ostringstream file;
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file.imbue(std::locale::classic());

        // 文件中的OBJECT ID只用于校验和人工阅读；加载时不会复用它，运行时ID继续由Scene递增分配。
        file << "INFINITE_SCENE " << kSceneFormatVersion << '\n';
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
                    if (&scene.objects_[candidateIndex]->transform == parent)
                    {
                        break;
                    }
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
}
