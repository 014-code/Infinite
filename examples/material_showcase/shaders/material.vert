#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

out vec2 uv;
out vec3 vertexColor;
out vec3 worldNormal;

void main()
{
    // 法线必须使用逆转置矩阵变换，才能在物体发生非均匀缩放时仍保持正确方向。
    worldNormal = normalize(normalMatrix * aNormal);
    vertexColor = aColor;
    uv = aUv;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
