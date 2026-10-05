#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 3) in vec3 aNormal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
out vec3 worldNormal;
out vec3 vertexColor;

void main()
{
    worldNormal = normalize(normalMatrix * aNormal);
    vertexColor = aColor;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
