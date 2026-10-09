#pragma once

#include <filesystem>

class Scene;

namespace scene_serialization_detail
{
    // SceneFileWriter把Scene快照写成版本化文本，并通过PendingSceneFile提交。
    // 所有校验在打开目标替换前完成，非法运行时资源不会截断旧文件。
    class SceneFileWriter final
    {
    public:
        static void write(const Scene &scene, const std::filesystem::path &path);
    };
}
