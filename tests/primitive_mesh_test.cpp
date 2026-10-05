#include "TestSupport.h"
#include "graphics/geometry/primitives/PrimitiveMeshBuilder.h"

#include <glm/geometric.hpp>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    void checkMesh(const PrimitiveDescription &description)
    {
        const auto mesh = PrimitiveMeshBuilder::build(description);
        require(!mesh.vertices.empty() && !mesh.indices.empty() && mesh.indices.size() % 3 == 0, "Empty primitive");
        for (const auto &v : mesh.vertices)
        {
            require(std::isfinite(v.position.x) && std::isfinite(v.position.y) && std::isfinite(v.position.z), "Invalid position");
            require(std::abs(glm::length(v.normal) - 1) < 0.0001f, "Non-unit normal");
            require(v.uv.x >= 0 && v.uv.x <= 1 && v.uv.y >= 0 && v.uv.y <= 1, "Invalid UV");
            // 检查三轴范围；只检查默认半径会漏掉size/height参数没有参与生成的问题。
            glm::vec3 extent(description.radius);
            switch (description.type)
            {
            case PrimitiveType::Cube: extent = description.size * .5f; break;
            case PrimitiveType::Plane: extent = {description.size.x * .5f, 0, description.size.z * .5f}; break;
            case PrimitiveType::Disk: extent.y = 0; break;
            case PrimitiveType::Cylinder:
            case PrimitiveType::Cone: extent.y = description.height * .5f; break;
            default: break;
            }
            for (int axis = 0; axis < 3; ++axis)
            {
                require(std::abs(v.position[axis]) <= extent[axis] + .0001f, "Incorrect primitive bounds");
            }
        }
        for (auto index : mesh.indices) { require(index < mesh.vertices.size(), "Index out of range"); }
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
        {
            const auto &a = mesh.vertices[mesh.indices[i]], &b = mesh.vertices[mesh.indices[i + 1]], &c = mesh.vertices[mesh.indices[i + 2]];
            // 用double叉积检查很小尺寸的面，避免测试自身的float平方下溢。
            const auto face = glm::cross(glm::dvec3(b.position) - glm::dvec3(a.position), glm::dvec3(c.position) - glm::dvec3(a.position));
            require(glm::length(face) > 0, "Degenerate triangle");
            require(glm::dot(face, glm::dvec3(a.normal + b.normal + c.normal)) > 0, "Winding disagrees with normals");
        }
    }
}

int main()
{
    try
    {
        for (const auto type : {PrimitiveType::Cube, PrimitiveType::Plane, PrimitiveType::Disk,
            PrimitiveType::Sphere, PrimitiveType::Cylinder, PrimitiveType::Cone})
        {
            PrimitiveDescription d; d.type = type;
            checkMesh(d);
            d.radialSegments = 3; d.latitudeSegments = 2; checkMesh(d);
            d.size = {2, 3, 4}; d.radius = 1.7f; d.height = 3.2f; checkMesh(d);
            d.radius = .0001f; d.height = .0001f; checkMesh(d);
        }
        PrimitiveDescription d;
        require(PrimitiveMeshBuilder::build(d).vertices.size() == 24, "Cube needs per-face vertices");
        d.type = PrimitiveType::Cylinder;
        const auto cylinder = PrimitiveMeshBuilder::build(d);
        require(cylinder.vertices.size() == 134 && cylinder.indices.size() == 384, "Cylinder side/cap count mismatch");
        require(cylinder.vertices[0].position == cylinder.vertices[64].position &&
            cylinder.vertices[0].uv.x == 0 && cylinder.vertices[64].uv.x == 1, "UV seam not closed");
        d.type = PrimitiveType::Sphere;
        require(PrimitiveMeshBuilder::build(d).indices.size() == 6 * 32 * 15, "Sphere pole triangles incorrect");
        d.radialSegments = 512; d.latitudeSegments = 256; checkMesh(d);
        for (float invalid : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            d = {}; d.type = PrimitiveType::Cylinder; d.radius = invalid;
            expectThrow<std::invalid_argument>([&] { PrimitiveMeshBuilder::build(d); }, "Invalid radius accepted");
        }
        d = {}; d.type = PrimitiveType::Sphere; d.radialSegments = std::numeric_limits<std::uint32_t>::max();
        expectThrow<std::invalid_argument>([&] { PrimitiveMeshBuilder::build(d); }, "Unbounded segments accepted");
        d.radialSegments = 32; d.latitudeSegments = 1;
        expectThrow<std::invalid_argument>([&] { PrimitiveMeshBuilder::build(d); }, "Invalid latitude count accepted");
        d.type = static_cast<PrimitiveType>(99);
        expectThrow<std::invalid_argument>([&] { PrimitiveMeshBuilder::build(d); }, "Unknown primitive accepted");
        d = {}; d.radius = std::numeric_limits<float>::quiet_NaN();
        require(PrimitiveMeshBuilder::canonicalize(d).radius == .5f, "Unused fields were not canonicalized");
        std::cout << "Primitive CPU geometry passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
