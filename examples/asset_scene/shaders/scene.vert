#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aUv;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec2 textureUv;

void main()
{
    // model包含父子变换，同一OBJ可以在不同位置、大小和朝向下绘制。
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    textureUv = aUv;
}
