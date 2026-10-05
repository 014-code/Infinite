#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <string>

class Material;

// 应用层创建选项；几何描述另存于PrimitiveDescription，不把颜色/名字放进网格缓存键。
struct PrimitiveOptions
{
    std::string name; // 空名字自动使用形状名。
    glm::vec4 color{0.8f, 0.8f, 0.8f, 1.0f};
    // 非空时直接共享用户材质，忽略color，不偷偷修改用户设置。
    std::shared_ptr<Material> material;
    // 与material二选一；有文件来源才能保存并重建自定义材质。相对路径以调用时工作目录为准。
    // Application已接好文件资源服务；独立Scene需使用Scene(primitives, resources)。
    std::filesystem::path materialPath;
};

struct CubeOptions : PrimitiveOptions { glm::vec3 size{1.0f}; };
struct PlaneOptions : PrimitiveOptions { glm::vec2 size{1.0f}; }; // 宽X、深Z。
struct DiskOptions : PrimitiveOptions { float radius = .5f; std::uint32_t radialSegments = 32; };
struct SphereOptions : DiskOptions { std::uint32_t latitudeSegments = 16; };
struct CylinderOptions : DiskOptions { float height = 1.0f; };
struct ConeOptions : CylinderOptions {};
