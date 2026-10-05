#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aNormal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
out vec3 worldNormal;
out vec3 worldPosition;
out vec3 vertexColor;
out vec2 uv;

void main()
{
    // 非均匀缩放需要逆转置法线矩阵；Renderer已经负责安全计算和上传。
    worldNormal = normalize(normalMatrix * aNormal);
    // 点光/聚光需要片段世界位置，不能只传法线；摄像机移动不应改变灯的位置。
    vec4 world = model * vec4(aPos, 1.0);
    worldPosition = world.xyz;
    vertexColor = aColor;
    uv = aUV;
    gl_Position = projection * view * world;
}
