#version 330 core

// 固定材质颜色，让测试能区分“绘制的红色”和“清屏的黑色”。
uniform vec4 baseColor;
out vec4 FragColor;

void main()
{
    FragColor = baseColor;
}
