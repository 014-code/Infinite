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
    // 颜色贴图在采样时已从sRGB解码。在线性空间计算光照，输出时只编码一次。
    return mix(12.92 * value, 1.055 * pow(max(value, vec3(0)), vec3(1.0 / 2.4)) - 0.055,
        step(vec3(0.0031308), value));
}

void main()
{
    // 这是示例用的基础颜色漫反射，不是完整PBR：纹理颜色乘以材质颜色，再乘以环境光和方向光。
    vec3 albedo = hasTexture ? texture(textureSampler, uv).rgb : vec3(1.0);
    float diffuse = max(dot(normalize(worldNormal), -normalize(lightDirection)), 0.0);
    vec3 lighting = ambientLight + lightColor * lightIntensity * diffuse;
    FragColor = vec4(linearToSrgb(albedo * baseColor.rgb * vertexColor * lighting), baseColor.a);
}
