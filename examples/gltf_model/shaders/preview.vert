#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUv;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec2 uv;
void main()
{
    // 节点层级由Transform组合，Shader只接收最终世界矩阵。
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    uv = aUv;
}
