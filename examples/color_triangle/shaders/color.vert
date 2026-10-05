#version 330 core

// 顶点位置和颜色输入。
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

// 接收CPU上传的模型、视图和投影矩阵。
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// 将颜色传递给片段着色器。
out vec3 vertexColor;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vertexColor = aColor;
}
