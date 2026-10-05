#include "PrimitiveMeshBuilder.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <stdexcept>
#include <string>

namespace
{
    void checkSize(float value, const char *field)
    {
        if (!std::isfinite(value) || value < 0.0001f || value > 1000000.0f)
        {
            throw std::invalid_argument(std::string("Primitive ") + field + " must be in [0.0001, 1000000]");
        }
    }

    // 接缝处强制回到起点，避免sin(2*pi)浮点误差造成细小裂缝。
    glm::vec3 radial(std::uint32_t index, std::uint32_t segments)
    {
        const float angle = index == segments ? 0.0f :
            glm::two_pi<float>() * static_cast<float>(index) / segments;
        return {std::cos(angle), 0.0f, std::sin(angle)};
    }

    void vertex(MeshData &mesh, const glm::vec3 &position, const glm::vec3 &normal, const glm::vec2 &uv)
    {
        mesh.vertices.push_back({position, glm::vec3(1.0f), uv, normal});
    }

    void quad(MeshData &mesh, const glm::vec3 &center, const glm::vec3 &u,
        const glm::vec3 &v, const glm::vec3 &normal)
    {
        const auto start = static_cast<std::uint32_t>(mesh.vertices.size());
        vertex(mesh, center - u * .5f - v * .5f, normal, {0, 0});
        vertex(mesh, center + u * .5f - v * .5f, normal, {1, 0});
        vertex(mesh, center + u * .5f + v * .5f, normal, {1, 1});
        vertex(mesh, center - u * .5f + v * .5f, normal, {0, 1});
        // 调用方保证cross(u, v)朝向normal，面剔除和光照才会一致。
        mesh.indices.insert(mesh.indices.end(), {start, start + 1, start + 2, start, start + 2, start + 3});
    }

    void cap(MeshData &mesh, float radius, float y, std::uint32_t segments, bool top)
    {
        const auto center = static_cast<std::uint32_t>(mesh.vertices.size());
        const glm::vec3 normal{0, top ? 1.0f : -1.0f, 0};
        vertex(mesh, {0, y, 0}, normal, {.5f, .5f});
        for (std::uint32_t i = 0; i <= segments; ++i)
        {
            const auto direction = radial(i, segments);
            vertex(mesh, {direction.x * radius, y, direction.z * radius}, normal,
                {.5f + direction.x * .5f, .5f - direction.z * .5f});
        }
        for (std::uint32_t i = 0; i < segments; ++i)
        {
            const auto current = center + 1 + i, next = current + 1;
            if (top) { mesh.indices.insert(mesh.indices.end(), {center, next, current}); }
            else { mesh.indices.insert(mesh.indices.end(), {center, current, next}); }
        }
    }

    MeshData sphere(const PrimitiveDescription &d)
    {
        MeshData mesh;
        const auto stride = d.radialSegments + 1;
        mesh.vertices.reserve((d.latitudeSegments + 1) * stride);
        mesh.indices.reserve(6 * d.radialSegments * (d.latitudeSegments - 1));
        for (std::uint32_t row = 0; row <= d.latitudeSegments; ++row)
        {
            const float v = static_cast<float>(row) / d.latitudeSegments;
            const float phi = glm::pi<float>() * v;
            for (std::uint32_t col = 0; col <= d.radialSegments; ++col)
            {
                auto normal = radial(col, d.radialSegments) * std::sin(phi);
                normal.y = std::cos(phi);
                // 两极严格重合到Y轴；每段保留自己的UV，但不输出退化三角形。
                if (row == 0) { normal = {0, 1, 0}; }
                if (row == d.latitudeSegments) { normal = {0, -1, 0}; }
                vertex(mesh, normal * d.radius, normal, {static_cast<float>(col) / d.radialSegments, 1 - v});
            }
        }
        for (std::uint32_t row = 0; row < d.latitudeSegments; ++row)
        {
            for (std::uint32_t col = 0; col < d.radialSegments; ++col)
            {
                const auto a = row * stride + col, b = a + stride;
                if (row != 0) { mesh.indices.insert(mesh.indices.end(), {a, a + 1, b}); }
                if (row + 1 != d.latitudeSegments) { mesh.indices.insert(mesh.indices.end(), {a + 1, b + 1, b}); }
            }
        }
        return mesh;
    }

    MeshData cylinder(const PrimitiveDescription &d)
    {
        MeshData mesh;
        mesh.vertices.reserve(4 * d.radialSegments + 6);
        mesh.indices.reserve(12 * d.radialSegments);
        for (std::uint32_t i = 0; i <= d.radialSegments; ++i)
        {
            const auto normal = radial(i, d.radialSegments);
            const float u = static_cast<float>(i) / d.radialSegments;
            vertex(mesh, {normal.x * d.radius, -d.height * .5f, normal.z * d.radius}, normal, {u, 0});
            vertex(mesh, {normal.x * d.radius, d.height * .5f, normal.z * d.radius}, normal, {u, 1});
        }
        for (std::uint32_t i = 0; i < d.radialSegments; ++i)
        {
            const auto a = i * 2, b = a + 2;
            mesh.indices.insert(mesh.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
        // 封盖与侧面不能共享顶点：同一位置有不同法线和UV，否则边缘会被错误磨圆。
        cap(mesh, d.radius, d.height * .5f, d.radialSegments, true);
        cap(mesh, d.radius, -d.height * .5f, d.radialSegments, false);
        return mesh;
    }

    MeshData cone(const PrimitiveDescription &d)
    {
        MeshData mesh;
        mesh.vertices.reserve(4 * d.radialSegments + 2);
        mesh.indices.reserve(6 * d.radialSegments);
        for (std::uint32_t i = 0; i < d.radialSegments; ++i)
        {
            const auto a = radial(i, d.radialSegments), b = radial(i + 1, d.radialSegments);
            const float slope = d.radius / d.height;
            const auto na = glm::normalize(glm::vec3(a.x, slope, a.z));
            const auto nb = glm::normalize(glm::vec3(b.x, slope, b.z));
            // 锥尖没有唯一表面法线，每个扇区使用中间方向，并独立保存纹理U。
            const auto middle = glm::normalize(a + b);
            const auto tipNormal = glm::normalize(glm::vec3(middle.x, slope, middle.z));
            const auto start = static_cast<std::uint32_t>(mesh.vertices.size());
            vertex(mesh, {a.x * d.radius, -d.height * .5f, a.z * d.radius}, na,
                {static_cast<float>(i) / d.radialSegments, 0});
            vertex(mesh, {0, d.height * .5f, 0}, tipNormal, {(i + .5f) / d.radialSegments, 1});
            vertex(mesh, {b.x * d.radius, -d.height * .5f, b.z * d.radius}, nb,
                {static_cast<float>(i + 1) / d.radialSegments, 0});
            mesh.indices.insert(mesh.indices.end(), {start, start + 1, start + 2});
        }
        cap(mesh, d.radius, -d.height * .5f, d.radialSegments, false);
        return mesh;
    }
}

PrimitiveDescription PrimitiveMeshBuilder::canonicalize(const PrimitiveDescription &d)
{
    PrimitiveDescription result;
    result.type = d.type;
    switch (d.type)
    {
    case PrimitiveType::Cube:
        checkSize(d.size.x, "size.x"); checkSize(d.size.y, "size.y"); checkSize(d.size.z, "size.z");
        result.size = d.size;
        break;
    case PrimitiveType::Plane:
        checkSize(d.size.x, "size.x"); checkSize(d.size.z, "size.z");
        result.size = {d.size.x, 1.0f, d.size.z};
        break;
    case PrimitiveType::Disk:
    case PrimitiveType::Sphere:
    case PrimitiveType::Cylinder:
    case PrimitiveType::Cone:
        checkSize(d.radius, "radius");
        if (d.radialSegments < 3 || d.radialSegments > 512)
        {
            throw std::invalid_argument("Primitive radialSegments must be in [3, 512]");
        }
        result.radius = d.radius;
        result.radialSegments = d.radialSegments;
        if (d.type == PrimitiveType::Sphere)
        {
            if (d.latitudeSegments < 2 || d.latitudeSegments > 256)
            {
                throw std::invalid_argument("Sphere latitudeSegments must be in [2, 256]");
            }
            result.latitudeSegments = d.latitudeSegments;
        }
        if (d.type == PrimitiveType::Cylinder || d.type == PrimitiveType::Cone)
        {
            checkSize(d.height, "height"); result.height = d.height;
        }
        break;
    default: throw std::invalid_argument("Unknown PrimitiveType");
    }
    // 上面的上限先校验再参与乘法：最大的球体也只有131841个顶点，不会整数溢出或巨量分配。
    return result;
}

MeshData PrimitiveMeshBuilder::build(const PrimitiveDescription &description)
{
    const auto d = canonicalize(description);
    if (d.type == PrimitiveType::Sphere) { return sphere(d); }
    if (d.type == PrimitiveType::Cylinder) { return cylinder(d); }
    if (d.type == PrimitiveType::Cone) { return cone(d); }
    MeshData mesh;
    if (d.type == PrimitiveType::Disk)
    {
        mesh.vertices.reserve(d.radialSegments + 2);
        mesh.indices.reserve(3 * d.radialSegments);
        cap(mesh, d.radius, 0, d.radialSegments, true);
    }
    else if (d.type == PrimitiveType::Plane)
    {
        quad(mesh, {0, 0, 0}, {d.size.x, 0, 0}, {0, 0, -d.size.z}, {0, 1, 0});
    }
    else
    {
        const auto h = d.size * .5f;
        mesh.vertices.reserve(24); mesh.indices.reserve(36);
        quad(mesh, {0, 0, h.z}, {d.size.x, 0, 0}, {0, d.size.y, 0}, {0, 0, 1});
        quad(mesh, {0, 0, -h.z}, {-d.size.x, 0, 0}, {0, d.size.y, 0}, {0, 0, -1});
        quad(mesh, {h.x, 0, 0}, {0, 0, -d.size.z}, {0, d.size.y, 0}, {1, 0, 0});
        quad(mesh, {-h.x, 0, 0}, {0, 0, d.size.z}, {0, d.size.y, 0}, {-1, 0, 0});
        quad(mesh, {0, h.y, 0}, {d.size.x, 0, 0}, {0, 0, -d.size.z}, {0, 1, 0});
        quad(mesh, {0, -h.y, 0}, {d.size.x, 0, 0}, {0, 0, d.size.z}, {0, -1, 0});
    }
    return mesh;
}
