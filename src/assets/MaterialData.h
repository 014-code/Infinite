#pragma once

#include "graphics/resources/Material.h"

#include <filesystem>

// MaterialData是材质文件解析后的CPU数据，不持有Shader或Texture的OpenGL对象。
// ResourceManager读取它后，再把路径交给对应资源加载器并构造运行时Material。
struct MaterialData
{
    std::filesystem::path vertexShaderPath;
    std::filesystem::path fragmentShaderPath;
    std::filesystem::path texturePath;
    bool hasTexture = false;
    glm::vec4 baseColor{1.0f};
    RenderMode renderMode = RenderMode::Opaque;
    CullMode cullMode = CullMode::None;
};
