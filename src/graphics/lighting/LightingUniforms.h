#pragma once

#include "SceneLighting.h"

class Shader;

// 每次绘制批次只准备一次：先校验，再归一化方向，避免每个物体重复做相同的CPU工作。
class LightingUniforms final
{
public:
    explicit LightingUniforms(const SceneLighting &lighting);
    // 不修改GL状态。旧受光Shader只能用单主灯，无光照Shader无需接受灯光参数。
    void validateShader(const Shader &shader) const;
    // 调用方先use该Shader；不存在的uniform遵循Shader原有的忽略约定。
    void upload(const Shader &shader) const;

private:
    SceneLighting lighting_;
};
