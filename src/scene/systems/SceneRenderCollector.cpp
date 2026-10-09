#include "SceneRenderCollector.h"

#include "scene/Scene.h"
#include "scene/SkinBinding.h"

std::vector<RenderItem> SceneRenderCollector::collect(const Scene &scene)
{
    // 预留物体数量，减少场景稳定后每帧收集快照的扩容次数。
    std::vector<RenderItem> items;
    items.reserve(scene.objects_.size());
    for (const auto &object : scene.objects_)
    {
        const Renderable &renderable = object->renderable();
        if (object->isActive() && renderable.isBound())
        {
            items.push_back({renderable.mesh(), renderable.material(), &object->transform,
                renderable.sortOrigin(), {}});
            if (renderable.skin())
            {
                items.back().skinMatrices = renderable.skin()->palette(scene, object->transform);
            }
        }
    }
    return items;
}
