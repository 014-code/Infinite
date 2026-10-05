#pragma once

#include "MeshData.h"

#include <filesystem>

// MeshLoader读取静态OBJ文件并生成MeshData，不创建OpenGL资源。
// 支持位置、UV、法线和负索引；多边形按扇形拆分，适用于平面凸面。
// 凹面建议在建模工具中先三角化；缺少法线时生成面法线，不根据平滑组生成平滑法线。
// 当前不解析MTL、骨骼动画或子网格分组；所有面合并为一个MeshData，材质单独加载。
class MeshLoader final
{
public:
    static MeshData loadObj(const std::filesystem::path &path);
};
