#pragma once

#include "GameObject.h"
#include "PrimitiveOptions.h"
#include "graphics/geometry/primitives/PrimitiveTypes.h"
#include "graphics/lighting/SceneLighting.h"
#include "graphics/rendering/RenderItem.h"

#include <memory>
#include <unordered_map>
#include <vector>

class Renderer;
class Camera;
class SceneSerializer;
class ModelInstantiator;
class PrimitiveResources;
class ResourceManager;
class PhysicsWorld;
struct DirectionalLight;
class SceneManager;
namespace scene_serialization_detail
{
    class SceneFileWriter;
    class SceneBuilder;
}
class SceneAudioSync;
class ScenePhysicsSync;
class SceneRenderCollector;
class SceneSystem;

// Scene拥有物体；物体可以借用资源，也可以通过shared_ptr共享持有资源。仅在主线程操作。
// 应用应先销毁Scene，再销毁资源，最后销毁提供OpenGL上下文的Window。
class Scene
{
public:
    Scene() = default;
    // 借用服务，不触发GPU创建。服务须比Scene活得更久；Application自动保证此顺序。
    explicit Scene(PrimitiveResources &resources) : primitiveResources_(&resources) {}
    Scene(PrimitiveResources &primitives, ResourceManager &files)
        : primitiveResources_(&primitives), fileResources_(&files) {}
    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;
    Scene(Scene &&) = delete;
    Scene &operator=(Scene &&) = delete;

    // 名称允许重复。返回借用引用，新增/删除其他物体不会改变这个物体的地址。
    // removeObject、clear或Scene析构会使对应物体的所有指针和引用失效。
    GameObject &createObject(const std::string &name);
    // 一行创建可绘制几何体。需要绑定服务和当前OpenGL上下文；不自动添加碰撞或调整镜头。
    // 与createObject一样，不能在Scene::update内部增删物体；创建失败不改变场景和ID。
    GameObject &createPrimitive(PrimitiveType type, const PrimitiveOptions &options = {});
    GameObject &createPrimitive(const PrimitiveDescription &description, const PrimitiveOptions &options = {});
    GameObject &createCube(const CubeOptions &options = {});
    GameObject &createPlane(const PlaneOptions &options = {});
    GameObject &createDisk(const DiskOptions &options = {});
    GameObject &createSphere(const SphereOptions &options = {});
    GameObject &createCylinder(const CylinderOptions &options = {});
    GameObject &createCone(const ConeOptions &options = {});
    GameObject *findObject(ObjectId id);
    const GameObject *findObject(ObjectId id) const;
    bool removeObject(ObjectId id);
    void clear();
    std::size_t objectCount() const;

    // 光照属于Scene，而不是某个Material。clear只清物体，保留场景环境配置。
    SceneLighting &lighting() noexcept { return lighting_; }
    const SceneLighting &lighting() const noexcept { return lighting_; }

    // 按创建顺序更新启用的物体，deltaTime单位是秒，必须有限且非负。
    // 更新中禁止create/remove/clear及重入；结构变更应在update返回后执行。
    // 回调异常传回应用入口，已更新物体的变更不会回滚。
    void update(float deltaTime);

    // 注册一个借用的场景级系统。Scene不负责系统的销毁；系统地址在注销前必须保持稳定。
    // 系统在所有GameObject脚本之后按注册顺序更新，适合实现跨物体的应用规则。
    void registerSystem(SceneSystem &system);
    // 返回是否找到并移除系统；更新过程中调用会抛出logic_error。
    bool unregisterSystem(SceneSystem &system);

    // 在PhysicsWorld每个固定子步开始前同步静态碰撞体。
    // 静态体的权威数据是GameObject的Transform，应用修改Transform后无需再手动逐个调用组件。
    void syncStaticPhysics(PhysicsWorld &world);
    // 在所有固定子步完成后，把动态刚体的插值位姿写回对应GameObject。
    // 动态体的权威数据是PhysicsWorld，应用如需瞬移应调用物理世界的位姿接口。
    void syncDynamicPhysics(PhysicsWorld &world, float interpolationAlpha);
    // 固定步完成后把角色组件的状态写回GameObject位置；角色朝向仍由应用层控制。
    void syncCharacterPhysics();
    // 固定物理步完成后更新所有Area并派发bodyEntered/bodyExited事件。
    void updateAreas();
    // 将带空间化音源的组件位置同步到AudioSystem；不负责创建或播放声音。
    void syncAudio();

    // 收集一次本帧快照供阴影/颜色通道共用。资源和Transform仍是借用，消费前不得增删场景。
    // 有蒙皮的对象同时解析关节ID并计算矩阵；删除关节会在任何绘制前报错。
    std::vector<RenderItem> renderItems() const;

    // 跳过禁用和空物体，交给Renderer统一绘制；不清屏、不更新逻辑、不交换缓冲。
    // 调用期间不得增删物体或销毁/移动其资源；窗口上下文必须有效。
    void render(Renderer &renderer, const Camera &camera, float aspectRatio) const;
    void render(Renderer &renderer, const Camera &camera, float aspectRatio,
        const DirectionalLight &light) const;
    // 显式覆盖用于测试或一次性绘制，不修改Scene持有的配置。
    void render(Renderer &renderer, const Camera &camera, float aspectRatio,
        const SceneLighting &lighting) const;

private:
    friend class SceneSerializer;
    friend class SceneManager;
    friend class scene_serialization_detail::SceneFileWriter;
    friend class scene_serialization_detail::SceneBuilder;
    friend class ModelInstantiator;
    friend class SceneAudioSync;
    friend class ScenePhysicsSync;
    friend class SceneRenderCollector;

    // vector移动的是智能指针，实际GameObject单独分配，因此扩容不会改变物体地址。
    std::vector<std::unique_ptr<GameObject>> objects_;
    // 这里只保存借用指针，不拥有GameObject；所有增删路径必须与objects_同步更新。
    // 这样按ID查找不需要每次扫描整个场景，Model/脚本/序列化恢复大量对象时更稳定。
    std::unordered_map<ObjectId, GameObject *> objectIndex_;
    ObjectId nextId_ = 1;
    bool updating_ = false;
    // 只借用外部系统，不参与Scene复制、清空或关卡对象的所有权管理。
    std::vector<SceneSystem *> systems_;
    SceneLighting lighting_;
    PrimitiveResources *primitiveResources_ = nullptr;
    ResourceManager *fileResources_ = nullptr;

    // SceneManager加载新关卡时使用：staging只继承服务绑定和光照配置，不复制旧物体。
    void prepareStaging(Scene &staging) const;
    // 只有staging完全构建成功后才调用，交换后旧物体由临时Scene负责析构。
    void replaceContents(Scene &staging);
};
