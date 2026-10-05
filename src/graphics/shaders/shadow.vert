#version 330 core
// @skin_capacity@
// @skinning@
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
layout(location=5) in float aAlpha;
uniform mat4 model, lightMatrix;
out vec2 uv;
out float vertexAlpha;
void main()
{
    uv = aUV;
    vertexAlpha = aAlpha;
    gl_Position = lightMatrix * model * skinTransform() * vec4(aPos,1);
}
