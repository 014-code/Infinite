#pragma once

#include "MaterialData.h"

#include <filesystem>

// MaterialLoader读取项目自己的版本化材质文本格式，不创建OpenGL对象。
// 路径字段相对于材质文件目录解析，便于整个assets目录移动到另一台机器。
class MaterialLoader final
{
public:
    static MaterialData load(const std::filesystem::path &path);
};
