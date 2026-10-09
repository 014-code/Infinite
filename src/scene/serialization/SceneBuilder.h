#pragma once

#include "scene/serialization/SceneData.h"
#include "scene/GameObject.h"

#include <filesystem>

class ResourceManager;
class Scene;

namespace scene_serialization_detail
{
    // SceneBuilder把纯CPU数据构建为临时Scene，成功后一次性替换目标Scene。
    // 这样资源加载、对象创建或父子绑定失败都不会清空调用方当前场景。
    class SceneBuilder final
    {
    public:
        static std::vector<ObjectId> replace(Scene &scene, const ObjectList &objects,
            ResourceManager *resources);
    };
}
