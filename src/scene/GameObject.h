#pragma once

#include "math/Transform.h"
#include "scene/components/Renderable.h"

#include <cstdint>
#include <functional>
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

    using UpdateCallback = std::function<void(GameObject &, float)>;
    // 注册应用逻辑；空回调表示不更新。不在框架里硬编码旋转或移动。
    // 捕获的引用必须保持有效；回调内不能增删Scene对象或递归update。
    void setUpdateCallback(UpdateCallback callback);

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
    // 执行中的回调和物体共同持有同一份函数对象，既允许自替换，也保留跨帧捕获状态。
    std::shared_ptr<UpdateCallback> updateCallback_;
    // 渲染资源和排序参考点已经封装到组件中，GameObject只保留组件本身。
    Renderable renderable_;
};
