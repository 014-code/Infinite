#include "Scene.h"

#include "graphics/rendering/Renderer.h"
#include "graphics/lighting/Lighting.h"
#include "physics/world/PhysicsWorld.h"
#include "SkinBinding.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

GameObject &Scene::createObject(const std::string &name)
{
    if (updating_)
    {
        throw std::logic_error("Cannot create objects during Scene::update");
    }
    if (nextId_ == 0)
    {
        throw std::overflow_error("Scene object IDs exhausted");
    }
    // 构造函数仅向Scene开放；先交给unique_ptr，再加入容器，异常时也不会泄漏。
    auto object = std::unique_ptr<GameObject>(new GameObject(nextId_, name));
    objects_.push_back(std::move(object));
    ++nextId_;
    return *objects_.back();
}

GameObject *Scene::findObject(ObjectId id)
{
    for (const auto &object : objects_)
    {
        if (object->id() == id)
        {
            return object.get();
        }
    }
    return nullptr;
}

const GameObject *Scene::findObject(ObjectId id) const
{
    for (const auto &object : objects_)
    {
        if (object->id() == id)
        {
            return object.get();
        }
    }
    return nullptr;
}

bool Scene::removeObject(ObjectId id)
{
    if (updating_)
    {
        throw std::logic_error("Cannot remove objects during Scene::update");
    }
    const auto found = std::find_if(objects_.begin(), objects_.end(),
        [id](const auto &object) { return object->id() == id; });
    if (found == objects_.end())
    {
        return false;
    }
    // 删除智能指针会自动销毁物体；借用资源不受影响，共享持有资源减少一份引用。
    objects_.erase(found);
    return true;
}

void Scene::clear()
{
    if (updating_)
    {
        throw std::logic_error("Cannot clear objects during Scene::update");
    }
    objects_.clear();
    // 不重置nextId_，否则旧ID可能误指向清空后新建的物体。
}

std::size_t Scene::objectCount() const
{
    return objects_.size();
}

void Scene::update(float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
    {
        throw std::invalid_argument("Scene deltaTime must be finite and nonnegative");
    }
    if (updating_)
    {
        throw std::logic_error("Scene::update cannot be called recursively");
    }
    updating_ = true;
    try
    {
        // 使用创建顺序更新，保证同一场景每帧行为稳定。脚本组件可以在回调中
        // 修改自己的Transform和回调，但Scene结构变化要等本次遍历结束。
        for (const auto &object : objects_)
        {
            if (object->isActive() && object->script().hasUpdateCallback())
            {
                object->script().update(*object, deltaTime);
            }
        }
    }
    catch (...)
    {
        updating_ = false;
        throw;
    }
    updating_ = false;
}

void Scene::syncStaticPhysics(PhysicsWorld &world)
{
    for (const auto &object : objects_)
    {
        if (object->physicsBody().belongsTo(world) && !object->physicsBody().isDynamic())
        {
            object->physicsBody().syncFromTransform(object->transform);
        }
    }
}

void Scene::syncDynamicPhysics(PhysicsWorld &world, float interpolationAlpha)
{
    for (const auto &object : objects_)
    {
        if (object->physicsBody().belongsTo(world) && object->physicsBody().isDynamic())
        {
            object->physicsBody().syncToTransform(object->transform, interpolationAlpha);
        }
    }
}

void Scene::syncCharacterPhysics()
{
    for (const auto &object : objects_)
    {
        if (object->characterBody().isAttached())
        {
            object->characterBody().syncToTransform(object->transform);
        }
    }
}

void Scene::updateAreas()
{
    for (const auto &object : objects_)
    {
        if (!object->isActive())
        {
            object->area().clearOverlaps();
            continue;
        }
        if (object->area().isAttached())
        {
            object->area().poll(object->transform);
        }
    }
}

void Scene::syncAudio()
{
    for (const auto &object : objects_)
    {
        if (object->isActive())
        {
            object->audioSource().syncTransform(object->transform);
        }
    }
}

void Scene::render(Renderer &renderer, const Camera &camera, float aspectRatio) const
{
    render(renderer, camera, aspectRatio, lighting_);
}

void Scene::render(Renderer &renderer, const Camera &camera, float aspectRatio,
    const DirectionalLight &light) const
{
    render(renderer, camera, aspectRatio, SceneLighting::fromDirectionalLight(light));
}

void Scene::render(Renderer &renderer, const Camera &camera, float aspectRatio,
    const SceneLighting &lighting) const
{
    renderer.drawItems(renderItems(), camera, aspectRatio, lighting);
}

std::vector<RenderItem> Scene::renderItems() const
{
    // Scene只负责把有资格绘制的对象转换成RenderItem；透明排序和OpenGL状态
    // 由Renderer统一处理，避免每个示例重复实现相同的渲染规则。
    std::vector<RenderItem> items;
    items.reserve(objects_.size());
    for (const auto &object : objects_)
    {
        const Renderable &renderable = object->renderable();
        if (object->isActive() && renderable.isBound())
        {
            items.push_back({renderable.mesh(), renderable.material(), &object->transform,
                renderable.sortOrigin(), {}});
            if (renderable.skin()) { items.back().skinMatrices = renderable.skin()->palette(*this, object->transform); }
        }
    }
    return items;
}
