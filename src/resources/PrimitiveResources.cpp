#include "PrimitiveResources.h"

#include "graphics/geometry/primitives/PrimitiveMeshBuilder.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Material.h"
#include "graphics/resources/Shader.h"
#include "PrimitiveShaderSources.h"
#include <GLFW/glfw3.h>

#include <cmath>
#include <stdexcept>
#include <tuple>

namespace
{
    void requireContext()
    {
        if (!glfwGetCurrentContext())
        {
            throw std::logic_error("PrimitiveResources requires a current OpenGL context");
        }
    }
}

bool PrimitiveResources::Less::operator()(const PrimitiveDescription &a, const PrimitiveDescription &b) const noexcept
{
    // 键已经过canonicalize：没有NaN，且无关字段相同，不会因为颜色/位置不同重复上传。
    return std::tie(a.type, a.size.x, a.size.y, a.size.z, a.radius, a.height, a.radialSegments, a.latitudeSegments) <
        std::tie(b.type, b.size.x, b.size.y, b.size.z, b.radius, b.height, b.radialSegments, b.latitudeSegments);
}

std::shared_ptr<const Mesh> PrimitiveResources::mesh(const PrimitiveDescription &description)
{
    const auto key = PrimitiveMeshBuilder::canonicalize(description);
    requireContext();
    const auto found = meshes_.find(key);
    if (found != meshes_.end()) { return found->second; }
    auto result = std::make_shared<Mesh>(PrimitiveMeshBuilder::build(key));
    meshes_.emplace(key, result);
    return result;
}

std::shared_ptr<Material> PrimitiveResources::createMaterial(PrimitiveType type, const glm::vec4 &color)
{
    PrimitiveDescription d; d.type = type;
    PrimitiveMeshBuilder::canonicalize(d);
    for (int channel = 0; channel < 4; ++channel)
    {
        if (!std::isfinite(color[channel])) { throw std::invalid_argument("Primitive color must be finite"); }
    }
    if (color.a < 0 || color.a > 1) { throw std::invalid_argument("Primitive alpha must be in [0, 1]"); }
    requireContext();
    if (!shader_)
    {
        shader_ = std::make_shared<Shader>(Shader::fromSource(
            PrimitiveShaders::vertex, PrimitiveShaders::fragment, "builtin primitive lighting"));
    }
    auto result = std::make_shared<Material>(shader_, color);
    result->setCullMode(type == PrimitiveType::Plane || type == PrimitiveType::Disk ? CullMode::None : CullMode::Back);
    result->setCorrectMirroredWinding(true);
    result->setRenderMode(color.a < 1 ? RenderMode::AlphaBlend : RenderMode::Opaque);
    return result;
}

void PrimitiveResources::clear() noexcept
{
    // 外部仍由Scene/Material共享持有的资源继续有效；清理缓存不等于强制销毁全部资源。
    meshes_.clear();
    shader_.reset();
}
