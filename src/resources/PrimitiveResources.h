#pragma once

#include "graphics/geometry/primitives/PrimitiveTypes.h"
#include <glm/vec4.hpp>
#include <map>
#include <memory>

class Mesh;
class Material;
class Shader;

// 程序生成资源独立于文件资源缓存。构造时不调用OpenGL，首次请求时才创建GPU对象。
// 仅在有当前OpenGL上下文的主线程使用；销毁顺序为Scene -> 本服务 -> Window。
class PrimitiveResources final
{
public:
    PrimitiveResources() = default;
    PrimitiveResources(const PrimitiveResources &) = delete;
    PrimitiveResources &operator=(const PrimitiveResources &) = delete;
    // 返回只读Mesh，调用方不能move走缓存中的GPU对象。
    std::shared_ptr<const Mesh> mesh(const PrimitiveDescription &description);
    // 每次返回独立材质，仅共享内置Shader；修改一种物体的颜色不会串到其他物体。
    std::shared_ptr<Material> createMaterial(PrimitiveType type, const glm::vec4 &color);
    void clear() noexcept;
    std::size_t meshCount() const noexcept { return meshes_.size(); }

private:
    struct Less
    {
        bool operator()(const PrimitiveDescription &a, const PrimitiveDescription &b) const noexcept;
    };
    std::map<PrimitiveDescription, std::shared_ptr<const Mesh>, Less> meshes_;
    std::shared_ptr<Shader> shader_;
};
