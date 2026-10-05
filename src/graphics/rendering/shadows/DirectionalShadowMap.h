#pragma once

#include "graphics/lighting/DirectionalShadowSettings.h"
#include "graphics/rendering/RenderItem.h"
#include <GL/glew.h>
#include <memory>

// 对深度资源的短期借用。只在所属ShadowMap下次render/销毁之前有效。
struct DirectionalShadowView
{
    GLuint texture = 0;
    glm::mat4 lightMatrix{1};
    float bias = .001f;
};

// 只渲染深度，不拥有Scene。Opaque/MASK投影，BLEND不投影；有蒙皮时复用同一份关节快照。
// 构造不访问GL；首次render分配资源，必须先于上下文销毁。
class DirectionalShadowMap final
{
public:
    DirectionalShadowMap();
    ~DirectionalShadowMap();
    DirectionalShadowMap(const DirectionalShadowMap &) = delete;
    DirectionalShadowMap &operator=(const DirectionalShadowMap &) = delete;
    DirectionalShadowView render(const std::vector<RenderItem> &items, const glm::vec3 &direction,
        const DirectionalShadowSettings &settings);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
