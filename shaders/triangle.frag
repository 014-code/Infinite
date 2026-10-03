#version 330 core

// 接收经过光栅化插值后的顶点颜色
in vec3 vertexColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vertexColor, 1.0);
}
