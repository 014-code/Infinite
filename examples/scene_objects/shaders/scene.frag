#version 330 core

in vec3 vertexColor;
in vec2 textureUv;
uniform sampler2D textureSampler;
uniform vec4 baseColor;
// Material每次绘制都会设置此开关，无纹理时直接使用顶点色和材质色。
uniform bool hasTexture;
out vec4 FragColor;

void main()
{
    // 保留纹理和材质的Alpha乘积。是否混合由Material的RenderMode显式决定。
    vec4 surface = hasTexture ? texture(textureSampler, textureUv) : vec4(1.0);
    FragColor = surface * vec4(vertexColor, 1.0) * baseColor;
}
