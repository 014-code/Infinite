#version 330 core

// 顶点位置和颜色输入
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

// 将颜色传递给片段着色器
out vec3 vertexColor;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    vertexColor = aColor;
}
