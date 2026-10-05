uniform bool shadowEnabled;
uniform sampler2D shadowDepth;
uniform mat4 shadowMatrix;
uniform float shadowBias;

float mainLightVisibility(vec3 position, vec3 normal, vec3 toLight)
{
    if (!shadowEnabled) { return 1.0; }
    vec4 projected = shadowMatrix * vec4(position,1);
    vec3 coordinate = projected.xyz / projected.w * .5 + .5;
    // 固定覆盖盒以外不投影，不能重复采样边缘产生大片假阴影。
    if (any(lessThan(coordinate,vec3(0))) || any(greaterThan(coordinate,vec3(1)))) { return 1.0; }
    float bias = max(shadowBias * (1.0 - dot(normal,toLight)), shadowBias * .25);
    vec2 texel = 1.0 / vec2(textureSize(shadowDepth,0));
    float visible = 0;
    for (int y=-1;y<=1;++y) { for (int x=-1;x<=1;++x)
    {
        float nearest = texture(shadowDepth,coordinate.xy + vec2(x,y)*texel).r;
        visible += coordinate.z - bias <= nearest ? 1.0 : 0.0;
    } }
    return visible / 9.0;
}
