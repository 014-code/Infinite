// 共享漫反射计算模块，由构建系统插入内置片段Shader，不能单独编译。
// 数量与SceneLighting的CPU限制一致；回归测试覆盖最大容量，避免两侧无声失配。
const int MAX_DIRECTIONAL_LIGHTS = 2;
const int MAX_POINT_LIGHTS = 8;
const int MAX_SPOT_LIGHTS = 4;

struct DirectionalSource
{
    vec3 direction;
    vec3 color;
    float intensity;
};

struct PointSource
{
    vec3 position;
    vec3 color;
    float intensity;
    float range;
};

struct SpotSource
{
    vec3 position;
    vec3 color;
    float intensity;
    float range;
    vec3 direction;
    float innerCos;
    float outerCos;
};

uniform vec3 ambientLight;
uniform int directionalLightCount;
uniform int pointLightCount;
uniform int spotLightCount;
uniform DirectionalSource directionalLights[MAX_DIRECTIONAL_LIGHTS];
uniform PointSource pointLights[MAX_POINT_LIGHTS];
uniform SpotSource spotLights[MAX_SPOT_LIGHTS];

float distanceAttenuation(float distanceToLight, float range)
{
    if (distanceToLight >= range) { return 0.0; }
    float ratio = distanceToLight / range;
    float fade = 1.0 - ratio * ratio * ratio * ratio;
    // 平方反比+范围边界平滑淡出；距离下限0.1避免光源附近出现无穷值。
    // 这是当前引擎的局部灯衰减约定，不宣称已经实现完整物理单位/PBR。
    return fade * fade / max(distanceToLight * distanceToLight, 0.01);
}

vec3 sceneDiffuse(vec3 position, vec3 normal)
{
    vec3 result = ambientLight;
    for (int i = 0; i < directionalLightCount; ++i)
    {
        float diffuse = max(dot(normal, -directionalLights[i].direction), 0.0);
        result += directionalLights[i].color * directionalLights[i].intensity * diffuse;
    }
    for (int i = 0; i < pointLightCount; ++i)
    {
        vec3 offset = pointLights[i].position - position;
        float distanceToLight = length(offset);
        // 光源与片段重合时方向没有定义，跳过直射项，不让normalize(0)污染整帧。
        if (distanceToLight < 1e-6) { continue; }
        float diffuse = max(dot(normal, offset / distanceToLight), 0.0);
        result += pointLights[i].color * pointLights[i].intensity * diffuse *
            distanceAttenuation(distanceToLight, pointLights[i].range);
    }
    for (int i = 0; i < spotLightCount; ++i)
    {
        vec3 offset = spotLights[i].position - position;
        float distanceToLight = length(offset);
        if (distanceToLight < 1e-6) { continue; }
        vec3 toLight = offset / distanceToLight;
        // direction从灯指向场景；-toLight也是灯到当前片段的方向。
        float cosine = dot(-toLight, spotLights[i].direction);
        float cone = smoothstep(spotLights[i].outerCos, spotLights[i].innerCos, cosine);
        float diffuse = max(dot(normal, toLight), 0.0);
        result += spotLights[i].color * spotLights[i].intensity * diffuse * cone *
            distanceAttenuation(distanceToLight, spotLights[i].range);
    }
    return result;
}
