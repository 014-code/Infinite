#include "SceneBuilder.h"

#include "resources/ResourceManager.h"
#include "scene/PrimitiveOptions.h"
#include "scene/Scene.h"

#include <stdexcept>

namespace scene_serialization_detail
{
    std::vector<ObjectId> SceneBuilder::replace(Scene &scene, const ObjectList &objects,
        ResourceManager *resources)
    {
        if (scene.updating_)
        {
            throw std::logic_error("Cannot load a Scene during Scene::update");
        }

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
            if (data.primitive)
            {
                object.setName(data.name);
            }
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
        for (const GameObject *object : loaded)
        {
            ids.push_back(object->id());
        }

        // 只有资源、对象和父子关系全部成功后才替换旧场景；失败路径不会触碰旧场景。
        // 由于loaded中的GameObject由loadedScene独占，移动objects_后父子Transform地址仍保持有效。
        scene.objects_ = std::move(loadedScene.objects_);
        scene.objectIndex_ = std::move(loadedScene.objectIndex_);
        scene.nextId_ = loadedScene.nextId_;
        return ids;
    }
}
