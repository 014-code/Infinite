#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aUv;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 vertexColor;
out vec2 textureUv;

void main()
{
    // 同一网格可以被多个物体复用，model由每个物体的Transform生成。
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vertexColor = aColor;
    textureUv = aUv;
}
