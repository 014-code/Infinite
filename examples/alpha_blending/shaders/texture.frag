#version 330 core

// 纹理本身Alpha定义边缘透明度，baseColor.a再整体缩放透明度。
in vec3 vertexColor;
in vec2 texCoord;
uniform sampler2D textureSampler;
uniform vec4 baseColor;
out vec4 FragColor;

void main()
{
    FragColor = texture(textureSampler, texCoord) * vec4(vertexColor, 1.0) * baseColor;
}
