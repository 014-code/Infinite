#pragma once

#include "Lighting.h"
#include <cstddef>
#include <vector>

// 新增光源只描述直射光；环境光属于场景，不应随着灯的数量重复相加。
struct DirectionalSource
{
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; // 世界空间中的光线传播方向。
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct PointLight
{
    glm::vec3 position{0.0f}; // 世界坐标；本阶段不自动绑定GameObject。
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float range = 10.0f; // 有限影响半径，范围之外贡献严格为0。
};

struct SpotLight : PointLight
{
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    // 圆锥半角，单位为弧度。内锥全亮，内外锥之间平滑衰减，外锥之外不照亮。
    float innerAngle = 0.35f;
    float outerAngle = 0.6f;
};

// Scene拥有这份纯CPU配置；不持有Shader/GPU对象，可单独测试、复制和编辑。
// 主灯保留稳定存储以兼容Application::directionalLight()返回的引用。
class SceneLighting
{
public:
    static constexpr std::size_t maxDirectionalLights = 2; // 包含主灯。
    static constexpr std::size_t maxPointLights = 8;
    static constexpr std::size_t maxSpotLights = 4;

    DirectionalLight &mainLight() noexcept { return mainLight_; }
    const DirectionalLight &mainLight() const noexcept { return mainLight_; }
    // legacy ambient字段是唯一存储，不另建一份环境光；主灯intensity=0不会关闭环境光。
    glm::vec3 &ambient() noexcept { return mainLight_.ambient; }
    const glm::vec3 &ambient() const noexcept { return mainLight_.ambient; }

    std::vector<DirectionalSource> additionalDirectionalLights;
    std::vector<PointLight> pointLights;
    std::vector<SpotLight> spotLights;

    // 将旧接口转换为只含一盏主灯的场景配置，保留原方向/颜色/环境光语义。
    static SceneLighting fromDirectionalLight(const DirectionalLight &light);
    // 全部先校验，数量超限直接报错，不静默截掉光源。无OpenGL调用。
    void validate() const;

private:
    DirectionalLight mainLight_;
};
