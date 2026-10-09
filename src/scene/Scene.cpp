#include "Scene.h"

#include "graphics/rendering/Renderer.h"
#include "graphics/lighting/Lighting.h"
#include "scene/systems/SceneAudioSync.h"
#include "scene/systems/ScenePhysicsSync.h"
#include "scene/systems/SceneRenderCollector.h"
#include "scene/systems/SceneSystem.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

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
    GameObject *const address = object.get();
    const ObjectId id = nextId_;
    // 先登记索引，再提交所有权；如果vector扩容失败，回滚索引即可保持两份数据一致。
    objectIndex_.emplace(id, address);
    try
    {
        objects_.push_back(std::move(object));
    }
    catch (...)
    {
        objectIndex_.erase(id);
        throw;
    }
    ++nextId_;
    return *address;
}

GameObject *Scene::findObject(ObjectId id)
{
    const auto found = objectIndex_.find(id);
    return found == objectIndex_.end() ? nullptr : found->second;
}

const GameObject *Scene::findObject(ObjectId id) const
{
    const auto found = objectIndex_.find(id);
    return found == objectIndex_.end() ? nullptr : found->second;
}

bool Scene::removeObject(ObjectId id)
{
    if (updating_)
    {
        throw std::logic_error("Cannot remove objects during Scene::update");
    }
    const auto indexed = objectIndex_.find(id);
    if (indexed == objectIndex_.end())
    {
        return false;
    }
    // vector仍按创建顺序保存对象，所以删除所有权需要一次线性定位；
    // 高频的按ID读取已经由objectIndex_降为平均O(1)，不改变稳定地址语义。
    const auto found = std::find_if(objects_.begin(), objects_.end(),
        [id](const auto &object) { return object->id() == id; });
    if (found == objects_.end())
    {
        // 这表示内部索引被破坏，宁可报告错误也不要留下悬空索引。
        objectIndex_.erase(indexed);
        throw std::logic_error("Scene object index is inconsistent");
    }
    objectIndex_.erase(indexed);
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
    objectIndex_.clear();
    objects_.clear();
    // 不重置nextId_，否则旧ID可能误指向清空后新建的物体。
}

void Scene::prepareStaging(Scene &staging) const
{
    if (&staging == this)
    {
        throw std::invalid_argument("Scene staging target must be a different Scene");
    }
    if (!staging.objects_.empty() || !staging.objectIndex_.empty() || staging.updating_)
    {
        throw std::logic_error("Scene staging target must be empty and idle");
    }

    // Loader需要和正式Scene使用同一组资源服务，否则提交后对象可能引用错误的资源缓存。
    staging.primitiveResources_ = primitiveResources_;
    staging.fileResources_ = fileResources_;
    // 旧的load会保留场景光照；先复制到staging，Loader仍可按关卡需要覆盖它。
    staging.lighting_ = lighting_;
    // 保持ID只增不复用，避免应用层保存的当前场景ID在切换后意外指向新物体。
    staging.nextId_ = nextId_;
}

void Scene::replaceContents(Scene &staging)
{
    if (&staging == this)
    {
        throw std::invalid_argument("Scene replacement source must be a different Scene");
    }
    if (updating_ || staging.updating_)
    {
        throw std::logic_error("Cannot replace a Scene while it is updating");
    }
    if (primitiveResources_ != staging.primitiveResources_ ||
        fileResources_ != staging.fileResources_)
    {
        throw std::logic_error("Scene replacement services do not match");
    }

    // map中的指针指向各自vector拥有的GameObject，因此必须和objects_一起交换。
    // GameObject本身在unique_ptr管理的独立地址中，不会因为容器交换而移动。
    objects_.swap(staging.objects_);
    objectIndex_.swap(staging.objectIndex_);
    std::swap(nextId_, staging.nextId_);
    std::swap(lighting_, staging.lighting_);
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
        // 系统在脚本完成后执行，能读取本帧脚本产生的Transform变化；注册顺序保持稳定。
        for (SceneSystem *system : systems_)
        {
            system->update(*this, deltaTime);
        }
    }
    catch (...)
    {
        updating_ = false;
        throw;
    }
    updating_ = false;
}

void Scene::registerSystem(SceneSystem &system)
{
    if (updating_)
    {
        throw std::logic_error("Cannot register a SceneSystem during Scene::update");
    }
    if (std::find(systems_.begin(), systems_.end(), &system) != systems_.end())
    {
        throw std::invalid_argument("SceneSystem is already registered");
    }
    systems_.push_back(&system);
}

bool Scene::unregisterSystem(SceneSystem &system)
{
    if (updating_)
    {
        throw std::logic_error("Cannot unregister a SceneSystem during Scene::update");
    }
    const auto found = std::find(systems_.begin(), systems_.end(), &system);
    if (found == systems_.end())
    {
        return false;
    }
    systems_.erase(found);
    return true;
}

void Scene::syncStaticPhysics(PhysicsWorld &world)
{
    ScenePhysicsSync::syncStatic(*this, world);
}

void Scene::syncDynamicPhysics(PhysicsWorld &world, float interpolationAlpha)
{
    ScenePhysicsSync::syncDynamic(*this, world, interpolationAlpha);
}

void Scene::syncCharacterPhysics()
{
    ScenePhysicsSync::syncCharacters(*this);
}

void Scene::updateAreas()
{
    ScenePhysicsSync::updateAreas(*this);
}

void Scene::syncAudio()
{
    SceneAudioSync::sync(*this);
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
    return SceneRenderCollector::collect(*this);
}
