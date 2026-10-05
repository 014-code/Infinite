#pragma once

#include <glm/vec3.hpp>
#include "graphics/geometry/primitives/PrimitiveTypes.h"

#include <memory>
#include <filesystem>
#include <optional>
#include <utility>

class Mesh;
class Material;
class ResourceManager;
struct SkinBinding;

// Renderable是GameObject的渲染组件。
//
// 它只描述“用什么几何形状和表面材质绘制这个物体”，不负责物体身份、Transform
// 或更新逻辑。Mesh是几何数据，Material是表面状态，两者在这里组合成一次可绘制配置。
// 组件本身可以借用外部资源，也可以通过shared_ptr持有资源，规则与原有GameObject
// 的setRenderable接口保持一致。
class Renderable final
{
public:
    Renderable() = default;
    Renderable(const Renderable &) = delete;
    Renderable &operator=(const Renderable &) = delete;
    Renderable(Renderable &&) = delete;
    Renderable &operator=(Renderable &&) = delete;

    // 同时绑定Mesh和Material，避免组件处于只有一半资源的状态。
    // 这个重载只借用资源，不负责销毁；调用者必须保证资源在绘制期间有效。
    void set(const Mesh &mesh, const Material &material);

    // 持有模式：组件保留两份shared_ptr，调用者离开当前作用域后资源仍然有效。
    // GPU资源仍须在OpenGL上下文销毁之前释放，shared_ptr不能替代上下文生命周期管理。
    void set(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material> material);

    // 禁止把临时资源交给借用接口，避免表达式结束后组件保存悬空指针。
    void set(const Mesh &&mesh, const Material &material) = delete;
    void set(const Mesh &mesh, const Material &&material) = delete;

    // 清除借用指针和shared_ptr持有关系；其他组件或调用者持有的资源不受影响。
    void clear();

    // 加载成功才替换原绑定，同时记录绝对资源路径，供场景保存时生成相对引用。
    // 缓存材质是共享实例，运行时修改不写回材质文件；序列化只保存文件引用。
    void setFromFiles(ResourceManager &resources, const std::filesystem::path &meshPath,
        const std::filesystem::path &materialPath);
    const std::filesystem::path &meshPath() const noexcept;
    const std::filesystem::path &materialPath() const noexcept;
    // 非空表示网格来自引擎生成器，可按参数重建。任意手工重新绑定都会清除此来源。
    const std::optional<PrimitiveDescription> &primitiveDescription() const noexcept { return primitive_; }
    bool hasBuiltinPrimitiveMaterial() const noexcept { return builtinPrimitiveMaterial_; }

    // 组件只有在Mesh和Material都存在时才是可绘制的。
    bool isBound() const noexcept;
    const Mesh *mesh() const noexcept;
    const Material *material() const noexcept;

    // 透明物体排序时使用的局部空间参考点，默认是局部原点。
    // 对于几何中心不在原点的网格，应用可以设置一个更合适的排序位置。
    const glm::vec3 &sortOrigin() const noexcept;
    void setSortOrigin(const glm::vec3 &origin) noexcept;
    // 蒙皮绑定属于实例而不是Material；重新绑定网格或clear会清除它。
    void setSkin(std::shared_ptr<const SkinBinding> skin) { skin_ = std::move(skin); }
    const SkinBinding *skin() const noexcept { return skin_.get(); }

private:
    friend class Scene;
    // 只有Scene的创建流程能标记来源，避免随意把任意Mesh谎报成某种基础几何体。
    std::optional<PrimitiveDescription> primitive_;
    bool builtinPrimitiveMaterial_ = false;
    std::filesystem::path meshPath_;
    std::filesystem::path materialPath_;
    const Mesh *mesh_ = nullptr;
    const Material *material_ = nullptr;
    std::shared_ptr<const Mesh> ownedMesh_;
    std::shared_ptr<const Material> ownedMaterial_;
    glm::vec3 sortOrigin_{0.0f};
    std::shared_ptr<const SkinBinding> skin_;
};
