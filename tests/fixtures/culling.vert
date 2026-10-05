#version 330 core

// 测试自己的最小着色器，仅变换顶点位置，不依赖示例Shader的颜色和纹理逻辑。
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
