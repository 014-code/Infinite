#version 330 core
in vec2 uv;
uniform vec4 baseColor;
uniform bool hasTexture;
uniform sampler2D textureSampler;
out vec4 FragColor;

vec3 linearToSrgb(vec3 value)
{
    // 标准分段sRGB传递函数，比简单pow(1/2.2)更准确，暗部也能正确显示。
    return mix(12.92 * value, 1.055 * pow(max(value, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
        step(vec3(0.0031308), value));
}

void main()
{
    // SRGB8纹理采样已转为线性颜色，在此乘线性的baseColorFactor后只编码一次。
    // Opaque模式忽略图片/因子的Alpha；没有光照、PBR和透明混合。
    vec3 surface = hasTexture ? texture(textureSampler, uv).rgb : vec3(1.0);
    FragColor = vec4(linearToSrgb(surface * baseColor.rgb), 1.0);
}
