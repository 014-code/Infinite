#version 330 core

// 接收顶点着色器传来的插值数据。
in vec3 vertexColor;
in vec2 texCoord;

// 纹理采样器和最终颜色输出。
uniform sampler2D textureSampler;
uniform vec4 baseColor;
out vec4 FragColor;

void main()
{
    // 让纹理颜色和材质颜色共同影响最终结果。
    FragColor = texture(textureSampler, texCoord) * vec4(vertexColor, 1.0) * baseColor;
}
