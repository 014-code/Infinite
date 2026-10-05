#version 330 core
// @skin_capacity@
// @skinning@

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec2 aUV;
layout(location=3) in vec3 aNormal;
layout(location=4) in vec4 aTangent;
layout(location=5) in float aAlpha;
uniform mat4 model, view, projection;
uniform mat3 normalMatrix;
out vec3 worldPosition;
out vec3 worldNormal;
out vec4 worldTangent;
out vec4 vertexColor;
out vec2 uv;
void main()
{
    mat4 skin = skinTransform();
    vec4 world = model * skin * vec4(aPos, 1);
    worldPosition = world.xyz;
    // 位置、法线和切线必须使用同一个蒙皮结果。法线用逆转置处理非均匀缩放。
    // LBS在极端姿态可能退化；退化面没有唯一法线，回退原法线避免NaN污染整帧。
    mat3 deformation = mat3(skin);
    float orientation = determinant(deformation);
    mat3 skinNormal = abs(orientation) > 1e-8 ? transpose(inverse(deformation)) : mat3(1);
    worldNormal = normalMatrix * skinNormal * aNormal;
    // 切线属于表面方向，用model而非逆转置；片段阶段再与法线正交化。
    // 镜像变换反转TBN手性，不能只修正OpenGL面剔除。
    worldTangent = vec4(mat3(model) * deformation * aTangent.xyz,
        aTangent.w * (determinant(mat3(model)) * orientation < 0 ? -1 : 1));
    vertexColor = vec4(aColor, aAlpha);
    uv = aUV;
    gl_Position = projection * view * world;
}
