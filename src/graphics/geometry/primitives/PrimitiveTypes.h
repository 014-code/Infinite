#pragma once

#include <glm/vec3.hpp>
#include <cstdint>

// 基础形状是引擎资源，不代表某个示例物体，也不自动附带碰撞体。
enum class PrimitiveType { Cube, Plane, Disk, Sphere, Cylinder, Cone };

// 只描述几何，不包含材质、位置或OpenGL句柄，可用于CPU生成、缓存键和场景存档。
// Y向上，外侧逆时针。Cube/Sphere在中心，Cylinder/Cone在轴线高度中点，Plane/Disk在XZ平面。
// 半径是理论圆周半径；低分段多边形的实际轴对齐包围盒不保证关于原点对称。
struct PrimitiveDescription
{
    PrimitiveType type = PrimitiveType::Cube;
    glm::vec3 size{1.0f};             // Cube的长宽高；Plane仅使用X/Z，其他形状忽略。
    float radius = 0.5f;             // Disk/Sphere/Cylinder/Cone的半径。
    float height = 1.0f;             // Cylinder/Cone的总高度，范围为[-height/2, height/2]。
    std::uint32_t radialSegments = 32; // 圆周分段数，3..512；越大越圆，不改变实际半径。
    std::uint32_t latitudeSegments = 16; // Sphere南北方向分段数，2..256。
};
