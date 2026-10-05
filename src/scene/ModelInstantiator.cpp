#include "ModelInstantiator.h"

#include "scene/Scene.h"
#include "scene/SkinBinding.h"

#include <stdexcept>

ModelInstance ModelInstantiator::instantiate(Scene &scene, const Model &model,
    const std::string &namePrefix)
{
    if (scene.updating_) { throw std::logic_error("Cannot instantiate model during Scene::update"); }
    ModelInstance result;
    result.owner = &scene;
    result.sourceModel = &model;
    const auto previousId = scene.nextId_;
    try
    {
        // 先为所有回滚ID预留容量，创建成功后登记ID不能再因vector扩容而抛异常。
        std::size_t count = 1 + model.nodes().size();
        for (const auto &node : model.nodes()) { count += node.primitives.size(); }
        result.nodeIds.reserve(model.nodes().size());
        result.objectIds.reserve(count);
        auto &root = scene.createObject(namePrefix);
        result.rootId = root.id();
        result.objectIds.push_back(root.id());
        for (const auto &node : model.nodes())
        {
            auto &object = scene.createObject(namePrefix + "/" + node.name);
            result.objectIds.push_back(object.id());
            object.transform.position = node.position;
            object.transform.setRotation(node.rotation);
            object.transform.scale = node.scale;
            result.nodeIds.push_back(object.id());
        }
        for (std::size_t index = 0; index < model.nodes().size(); ++index)
        {
            const auto parent = model.nodes()[index].parent;
            if (parent >= -1)
            {
                auto *child = scene.findObject(result.nodeIds[index]);
                auto *parentObject = scene.findObject(parent < 0 ? result.rootId : result.nodeIds.at(static_cast<std::size_t>(parent)));
                if (child == nullptr || parentObject == nullptr)
                {
                    throw std::logic_error("Model node parent index is invalid");
                }
                child->transform.setParent(&parentObject->transform);
            }
        }
        std::vector<std::shared_ptr<const SkinBinding>> skins;
        for (const auto &source : model.skins())
        {
            auto binding = std::make_shared<SkinBinding>();
            binding->inverseBind = source.inverseBind;
            for (auto joint : source.joints) { binding->joints.push_back(result.nodeIds.at(joint)); }
            skins.push_back(std::move(binding));
        }
        for (std::size_t nodeIndex = 0; nodeIndex < model.nodes().size(); ++nodeIndex)
        {
            const auto &node = model.nodes()[nodeIndex];
            for (std::size_t primitiveIndex : node.primitives)
            {
                if (primitiveIndex >= model.primitives().size())
                {
                    throw std::logic_error("Model primitive index is invalid");
                }
                auto &object = scene.createObject(namePrefix + "/" + node.name + "/primitive_" +
                    std::to_string(primitiveIndex));
                result.objectIds.push_back(object.id());
                object.setRenderable(model.primitives()[primitiveIndex].mesh,
                    model.primitives()[primitiveIndex].material);
                if (node.skin >= 0) { object.renderable().setSkin(skins.at(static_cast<std::size_t>(node.skin))); }
                auto *parentObject = scene.findObject(result.nodeIds[nodeIndex]);
                if (parentObject == nullptr) { throw std::logic_error("Model node disappeared"); }
                object.transform.setParent(&parentObject->transform);
            }
        }
        return result;
    }
    catch (...)
    {
        // 父对象先创建、绘制子对象后创建；逆序撤销这次创建，已有场景保持不变。
        for (auto it = result.objectIds.rbegin(); it != result.objectIds.rend(); ++it)
        {
            scene.removeObject(*it);
        }
        scene.nextId_ = previousId;
        throw;
    }
}

void ModelInstantiator::remove(Scene &scene, ModelInstance &instance)
{
    if (instance.owner != &scene) { throw std::invalid_argument("Model instance belongs to another Scene"); }
    if (scene.updating_) { throw std::logic_error("Cannot remove model during Scene::update"); }
    for (auto it = instance.objectIds.rbegin(); it != instance.objectIds.rend(); ++it) { scene.removeObject(*it); }
    instance.objectIds.clear(); instance.nodeIds.clear(); instance.rootId = 0;
}
