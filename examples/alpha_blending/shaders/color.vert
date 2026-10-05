#version 330 core

// 顶点位置和颜色输入；示例几何体提供白色顶点颜色。
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

// Renderer按每个物体上传模型矩阵，以及当前摄像机矩阵。
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 vertexColor;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vertexColor = aColor;
}
