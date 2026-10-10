#include "SceneRenderCollector.h"

#include "scene/Scene.h"
#include "scene/SkinBinding.h"

std::vector<RenderItem> SceneRenderCollector::collect(const Scene &scene)
{
    std::vector<RenderItem> items;
    collect(scene, items);
    return items;
}

void SceneRenderCollector::collect(const Scene &scene, std::vector<RenderItem> &output)
{
    // 预留物体数量，减少场景稳定后每帧收集快照的扩容次数；
    // 如果调用方跨帧复用容器，reserve不会缩小已经存在的容量。
    output.clear();
    output.reserve(scene.objects_.size());
    for (const auto &object : scene.objects_)
    {
        const Renderable &renderable = object->renderable();
        if (object->isActive() && renderable.isBound())
        {
            output.push_back({renderable.mesh(), renderable.material(), &object->transform,
                renderable.sortOrigin(), {}});
            if (renderable.skin())
            {
                output.back().skinMatrices = renderable.skin()->palette(scene, object->transform);
            }
        }
    }
}
