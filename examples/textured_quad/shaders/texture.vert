#version 330 core

// 顶点位置、颜色和纹理坐标输入。
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

// 接收CPU上传的模型、视图和投影矩阵。
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// 将颜色和纹理坐标传递给片段着色器。
out vec3 vertexColor;
out vec2 texCoord;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vertexColor = aColor;
    texCoord = aTexCoord;
}
