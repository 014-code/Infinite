#pragma once

#include "graphics/materials/PbrParameters.h"
#include <glm/vec4.hpp>
#include <memory>

class Shader;
class Material;

// 按需编译内置PBR Shader；每次创建独立材质，但底层Shader可共享。
// 与其他GPU服务一样，必须在有效上下文中使用并先于Window销毁。
class PbrResources final
{
public:
    std::shared_ptr<Shader> shader();
    std::shared_ptr<Material> createMaterial(const glm::vec4 &color = glm::vec4(1),
        const PbrParameters &parameters = {});
    // 只释放PbrResources自己的Shader引用；已有Material或Model仍在使用时保留Shader。
    void unloadUnused() noexcept;
    void clear() { shader_.reset(); }
private:
    std::shared_ptr<Shader> shader_;
};
