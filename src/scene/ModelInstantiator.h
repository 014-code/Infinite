#pragma once

#include "resources/Model.h"
#include "scene/GameObject.h"
#include <string>
#include <vector>

class Scene;

struct ModelInstance
{
    // 额外根对象用于一次移动/旋转整个实例，包括glTF中原本有多个根节点的情况。
    ObjectId rootId = 0;
    // objectIds包含节点对象和每个primitive的渲染对象，按创建顺序返回。
    // nodeIds只包含代表原始glTF节点的对象，应用行为通常绑定这些ID。
    std::vector<ObjectId> objectIds;
    std::vector<ObjectId> nodeIds;
private:
    friend class ModelInstantiator;
    friend class AnimationPlayer;
    Scene *owner = nullptr;
    // 仅用于绑定身份检查，不通过此指针访问Model；播放器自己持有shared_ptr保活。
    const Model *sourceModel = nullptr;
};

// 把共享Model展开为Scene对象。一个节点一个空GameObject，一个primitive一个渲染子对象，
// 这样不需要立刻改变Renderable“一份Mesh+一份Material”的现有职责。
class ModelInstantiator final
{
public:
    static ModelInstance instantiate(Scene &scene, const Model &model,
        const std::string &namePrefix = "Model");
    // 显式移除实例中的对象。Scene必须仍存活；不在析构中自动操作Scene，避免生命周期倒置。
    static void remove(Scene &scene, ModelInstance &instance);
};
