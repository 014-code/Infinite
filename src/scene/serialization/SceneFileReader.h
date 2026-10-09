#pragma once

#include "scene/serialization/SceneData.h"

#include <filesystem>

namespace scene_serialization_detail
{
    // SceneFileReader只负责把版本化文本转换为CPU中间数据。
    // 它不创建Scene对象、不加载GPU资源，因此可以在无OpenGL上下文时测试。
    class SceneFileReader final
    {
    public:
        static ObjectList read(const std::filesystem::path &path);
    };
}
