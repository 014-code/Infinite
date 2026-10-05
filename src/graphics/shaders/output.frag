#version 330 core
in vec2 uv;
uniform sampler2D linearImage;
uniform float exposure;
uniform bool toneMapping;
out vec4 FragColor;
void main()
{
    vec4 source = texture(linearImage, uv);
    vec3 color = max(source.rgb * exposure, vec3(0));
    if (toneMapping) { color = color / (vec3(1) + color); } // Reinhard，显式可关闭供参考像素测试。
    color = mix(12.92 * color, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055,
        step(vec3(0.0031308), color));
    FragColor = vec4(color, source.a); // Alpha不做Gamma或色调映射。
}
