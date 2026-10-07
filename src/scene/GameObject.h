#pragma once

#include "math/Transform.h"
#include "scene/components/AudioSourceComponent.h"
#include "scene/components/AreaComponent.h"
#include "scene/components/CharacterBodyComponent.h"
#include "scene/components/PhysicsBodyComponent.h"
#include "scene/components/Renderable.h"
#include "scene/components/ScriptComponent.h"

#include <cstdint>
#include <memory>
#include <string>

class Mesh;
class Material;
class Scene;

// ID只在所属Scene中有效，0表示无效ID；不同Scene可能分配相同数值。
using ObjectId = std::uint64_t;

class GameObject
{
public:
    // 每个物体有独立变换；共享Mesh不会共享位置、旋转和缩放。
    Transform transform;

    ObjectId id() const;
    // 名称只用于调试和展示，不作为唯一键；改名不会改变ID。
    const std::string &name() const;
    void setName(const std::string &name);
    // active影响Scene更新和绘制，不会阻止应用自己修改transform。
    bool isActive() const;
    void setActive(bool active);

    using UpdateCallback = ScriptComponent::UpdateCallback;
    // 注册应用逻辑；空回调表示不更新。不在框架里硬编码旋转或移动。
    // 捕获的引用必须保持有效；回调内不能增删Scene对象或递归update。
    void setUpdateCallback(UpdateCallback callback);
    // 访问脚本组件。组件本身不强制使用某种脚本语言，当前实现是C++回调。
    ScriptComponent &script();
    const ScriptComponent &script() const;

    // 绑定场景中的持续音源。一次性音效直接使用Application::audio()，不必为它创建组件。
    void setAudioSource(AudioSystem &audio, std::shared_ptr<const AudioClip> clip);
    void clearAudioSource() noexcept;
    AudioSourceComponent &audioSource();
    const AudioSourceComponent &audioSource() const;

    // 同时绑定网格和材质，避免只有一半配置。只借用资源，不负责销毁。
    // 资源及Material引用的Shader/Texture必须在绘制期间有效，且不能被移动。
    void setRenderable(const Mesh &mesh, const Material &material);
    // 推荐给跨作用域的应用对象：共享持有资源，不再要求调用者保留局部变量。
    // 两个参数都不能为空；共享资源不可被move走，Scene仍须先于Window销毁。
    void setRenderable(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material> material);
    // 防止把临时对象交给借用接口，在表达式结束后立刻悬空。
    void setRenderable(const Mesh &&mesh, const Material &material) = delete;
    void setRenderable(const Mesh &mesh, const Material &&material) = delete;
    void clearRenderable();

    // 访问渲染组件。组件始终存在，但在绑定Mesh和Material之前处于空状态。
    // 直接访问组件适合需要同时调整多个渲染属性的代码；下面的兼容接口仍可继续使用。
    Renderable &renderable();
    const Renderable &renderable() const;

    const Mesh *mesh() const;
    const Material *material() const;

    // 局部空间的透明排序参考点，默认是原点；非居中网格可指定自己的中心。
    const glm::vec3 &sortOrigin() const;
    void setSortOrigin(const glm::vec3 &origin);

    // 物理组件：以物体的当前位姿把形状注册为静态碰撞体。
    // 借用外部PhysicsWorld，世界必须比Scene和物体活得更久；平面形状请直接注册到世界。
    // 世界销毁后组件仍指向旧世界指针，因此不允许世界先于Scene销毁。
    void setPhysicsBody(PhysicsWorld &world, const CollisionShape &shape, const PhysicsFilter &filter = {});
    // 把物体注册为动态刚体。注册后物理世界拥有位姿写入权，应用应通过速度、冲量或瞬移接口控制它。
    void setDynamicPhysicsBody(PhysicsWorld &world, const CollisionShape &shape,
        const RigidBodySettings &settings = {}, const PhysicsFilter &filter = {});
    // 绑定角色控制组件。组件不接管输入，应用应在fixedUpdate中设置速度并调用move。
    // 物理Transform暂不支持父节点和非单位缩放，避免局部坐标/世界坐标和碰撞尺寸不一致。
    void setCharacterBody(PhysicsWorld &world, const CharacterSettings &settings = {},
        std::uint32_t queryMask = 0xFFFFFFFFu);
    // 注销静态碰撞体；物体被删除时组件析构同样会注销。
    void clearPhysicsBody();
    PhysicsBodyComponent &physicsBody();
    const PhysicsBodyComponent &physicsBody() const;
    CharacterBodyComponent &characterBody();
    const CharacterBodyComponent &characterBody() const;

    // Area是查询区域，不参与碰撞响应；事件在固定物理步完成后由Scene统一派发。
    void setArea(PhysicsWorld &world, const CollisionShape &shape,
        std::uint32_t queryMask = 0xFFFFFFFFu);
    void clearArea() noexcept;
    AreaComponent &area();
    const AreaComponent &area() const;

    // 物体的身份和地址由Scene管理，不允许复制或移动出另一个同ID物体。
    GameObject(const GameObject &) = delete;
    GameObject &operator=(const GameObject &) = delete;
    GameObject(GameObject &&) = delete;
    GameObject &operator=(GameObject &&) = delete;

private:
    friend class Scene;
    GameObject(ObjectId id, const std::string &name);

    ObjectId id_;
    std::string name_;
    bool active_ = true;
    // 应用层逻辑作为独立组件保存；GameObject只提供访问入口，不直接执行它。
    ScriptComponent script_;
    // 声音组件只持有Clip和Voice句柄，AudioSystem负责设备和混音。
    AudioSourceComponent audioSource_;
    // 渲染资源和排序参考点已经封装到组件中，GameObject只保留组件本身。
    Renderable renderable_;
    // 物理注册关系同样封装在组件里；物体被Scene删除时由组件析构注销静态体。
    PhysicsBodyComponent physicsBody_;
    CharacterBodyComponent characterBody_;
    AreaComponent area_;
};
