#pragma once

#include "graphics/rendering/RenderItem.h"

#include <vector>

class Scene;

// SceneRenderCollector把GameObject的组件状态转换为一份本帧渲染快照。
// 快照只借用Mesh、Material和Transform；调用方必须在消费结束前保持Scene及资源有效。
class SceneRenderCollector final
{
public:
    // 跳过禁用物体和未完成Mesh/Material绑定的物体。
    // 蒙皮矩阵在收集阶段计算，使Renderer不需要知道Scene或关节ID。
    static std::vector<RenderItem> collect(const Scene &scene);
};
