#version 330 core

in vec3 worldNormal;
in vec3 worldPosition;
in vec3 vertexColor;
in vec2 uv;
uniform vec4 baseColor;
uniform bool hasTexture;
uniform sampler2D textureSampler;
out vec4 FragColor;

// 下方标记在CMake构建时展开为scene_lighting.glsl，避免复制多份光照算法。
// @scene_lighting@

void main()
{
    vec4 surface = baseColor * vec4(vertexColor, 1.0);
    if (hasTexture) { surface *= texture(textureSampler, uv); }
    // 默认平面双面可见；背面翻转光照法线，不改变几何体本身的正面约定。
    vec3 normal = normalize(worldNormal);
    if (!gl_FrontFacing) { normal = -normal; }
    // 与现有基础漫反射路径一致：不包含PBR、高光或额外Gamma编码。
    FragColor = vec4(surface.rgb * sceneDiffuse(worldPosition, normal), surface.a);
}
