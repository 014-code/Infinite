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
    // 法线与光方向统一在世界空间比较，摄像机移动不会改变受光强弱。
    // 先归一化减少顶点间长度差异；片段插值后还需要再次归一化。
    worldNormal = normalize(normalMatrix * aNormal);
    vertexColor = aColor;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
