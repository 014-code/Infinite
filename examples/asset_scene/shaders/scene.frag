#version 330 core

in vec2 textureUv;
uniform sampler2D textureSampler;
uniform bool hasTexture;
uniform vec4 baseColor;
out vec4 FragColor;

void main()
{
    // 材质文件决定是否使用纹理以及基础颜色，本示例不引入额外光照功能。
    vec4 surface = hasTexture ? texture(textureSampler, textureUv) : vec4(1.0);
    FragColor = surface * baseColor;
}
