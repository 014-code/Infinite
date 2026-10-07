#version 330 core

in vec2 uv;
in vec3 vertexColor;
in vec3 worldNormal;

uniform vec4 baseColor;
uniform bool hasTexture;
uniform sampler2D textureSampler;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform vec3 ambientLight;

out vec4 FragColor;

vec3 linearToSrgb(vec3 value)
{
    // 图片按sRGB采样后转为线性空间参与漫反射，最后只编码一次输出。
    return mix(12.92 * value,
        1.055 * pow(max(value, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
        step(vec3(0.0031308), value));
}

void main()
{
    // 小游戏只需要基础颜色漫反射，复杂PBR仍由专门的PBR示例演示。
    vec3 albedo = hasTexture ? texture(textureSampler, uv).rgb : vec3(1.0);
    float diffuse = max(dot(normalize(worldNormal), -normalize(lightDirection)), 0.0);
    vec3 lighting = ambientLight + lightColor * lightIntensity * diffuse;
    FragColor = vec4(linearToSrgb(albedo * baseColor.rgb * vertexColor * lighting), baseColor.a);
}
