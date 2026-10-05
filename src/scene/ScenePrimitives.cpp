#include "Scene.h"

#include "resources/PrimitiveResources.h"
#include "resources/ResourceManager.h"
#include "graphics/geometry/primitives/PrimitiveMeshBuilder.h"
#include "graphics/resources/Material.h"
#include <cmath>
#include <stdexcept>

namespace
{
    const char *defaultName(PrimitiveType type)
    {
        switch (type)
        {
        case PrimitiveType::Cube: return "Cube";
        case PrimitiveType::Plane: return "Plane";
        case PrimitiveType::Disk: return "Disk";
        case PrimitiveType::Sphere: return "Sphere";
        case PrimitiveType::Cylinder: return "Cylinder";
        case PrimitiveType::Cone: return "Cone";
        default: throw std::invalid_argument("Unknown PrimitiveType");
        }
    }
}

GameObject &Scene::createPrimitive(PrimitiveType type, const PrimitiveOptions &options)
{
    PrimitiveDescription description; description.type = type;
    return createPrimitive(description, options);
}

GameObject &Scene::createPrimitive(const PrimitiveDescription &description, const PrimitiveOptions &options)
{
    if (updating_) { throw std::logic_error("Cannot create primitives during Scene::update"); }
    if (nextId_ == 0) { throw std::overflow_error("Scene object IDs exhausted"); }
    if (!primitiveResources_) { throw std::logic_error("Scene has no PrimitiveResources; use Application::scene or Scene(resources)"); }
    const auto canonical = PrimitiveMeshBuilder::canonicalize(description);
    if (options.material && !options.materialPath.empty())
    {
        throw std::invalid_argument("Specify either material or materialPath, not both");
    }
    if (!options.materialPath.empty() && !fileResources_)
    {
        throw std::logic_error("Primitive materialPath requires a ResourceManager bound to Scene");
    }
    const bool builtinMaterial = !options.material && options.materialPath.empty();
    if (builtinMaterial)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (!std::isfinite(options.color[i])) { throw std::invalid_argument("Primitive color must be finite"); }
        }
        if (options.color.a < 0 || options.color.a > 1) { throw std::invalid_argument("Primitive alpha must be in [0, 1]"); }
    }
    // 先准备所有可能失败的资源。失败可以保留已完成的缓存，但场景对象和ID不变。
    auto mesh = primitiveResources_->mesh(canonical);
    std::filesystem::path materialPath;
    auto material = options.material;
    if (!options.materialPath.empty())
    {
        materialPath = std::filesystem::absolute(options.materialPath).lexically_normal();
        material = fileResources_->loadMaterial(materialPath);
    }
    else if (!material)
    {
        material = primitiveResources_->createMaterial(canonical.type, options.color);
    }
    auto &object = createObject(options.name.empty() ? defaultName(canonical.type) : options.name);
    // 两份指针已保证非空，set只转移shared_ptr，不再分配GPU资源。
    object.setRenderable(std::move(mesh), std::move(material));
    // 保存可重建描述，而不是GPU句柄。只在绑定成功后提交，不涉及额外内存分配。
    object.renderable().primitive_ = canonical;
    object.renderable().builtinPrimitiveMaterial_ = builtinMaterial;
    object.renderable().materialPath_.swap(materialPath);
    return object;
}

GameObject &Scene::createCube(const CubeOptions &options)
{
    PrimitiveDescription d; d.type = PrimitiveType::Cube; d.size = options.size;
    return createPrimitive(d, options);
}

GameObject &Scene::createPlane(const PlaneOptions &options)
{
    PrimitiveDescription d; d.type = PrimitiveType::Plane; d.size = {options.size.x, 1, options.size.y};
    return createPrimitive(d, options);
}

GameObject &Scene::createDisk(const DiskOptions &options)
{
    PrimitiveDescription d; d.type = PrimitiveType::Disk; d.radius = options.radius; d.radialSegments = options.radialSegments;
    return createPrimitive(d, options);
}

GameObject &Scene::createSphere(const SphereOptions &options)
{
    PrimitiveDescription d; d.type = PrimitiveType::Sphere; d.radius = options.radius;
    d.radialSegments = options.radialSegments; d.latitudeSegments = options.latitudeSegments;
    return createPrimitive(d, options);
}

GameObject &Scene::createCylinder(const CylinderOptions &options)
{
    PrimitiveDescription d; d.type = PrimitiveType::Cylinder; d.radius = options.radius;
    d.height = options.height; d.radialSegments = options.radialSegments;
    return createPrimitive(d, options);
}

GameObject &Scene::createCone(const ConeOptions &options)
{
    PrimitiveDescription d; d.type = PrimitiveType::Cone; d.radius = options.radius;
    d.height = options.height; d.radialSegments = options.radialSegments;
    return createPrimitive(d, options);
}
