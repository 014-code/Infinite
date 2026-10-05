#version 330 core
in vec2 uv;
in float vertexAlpha;
uniform bool alphaMask, hasTexture;
uniform float baseAlpha, alphaCutoff;
uniform sampler2D textureSampler;
void main()
{
    // 镂空表面的阴影必须和颜色通道采用同一套Alpha，否则叶片会投出整块方形阴影。
    float alpha = baseAlpha * vertexAlpha * (hasTexture ? texture(textureSampler,uv).a : 1.0);
    if (alphaMask && alpha < alphaCutoff) { discard; }
}
