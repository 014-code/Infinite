#version 330 core

in vec3 worldPosition, worldNormal;
in vec4 worldTangent, vertexColor;
in vec2 uv;
uniform vec4 baseColor;
uniform bool hasTexture, hasMetallicRoughness, hasNormalTexture, hasOcclusion, hasEmission, unlit;
uniform sampler2D textureSampler, metallicRoughnessTexture, normalTexture, occlusionTexture, emissionTexture;
uniform float metallicFactor, roughnessFactor, normalScale, occlusionStrength, alphaCutoff;
uniform int alphaMode;
uniform vec3 emissionFactor, cameraPosition;
out vec4 FragColor;
// @scene_lighting@
// @pbr_brdf@
// @pbr_normal@
// @directional_shadow@

void main()
{
    vec4 surface = baseColor * vertexColor;
    if (hasTexture) { surface *= texture(textureSampler, uv); }
    if (alphaMode == 1 && surface.a < alphaCutoff) { discard; }
    float alpha = alphaMode == 2 ? surface.a : 1.0;
    // unlit保留基础颜色，不叠加环境、法线、金属度或发光。
    if (unlit) { FragColor = vec4(surface.rgb, alpha); return; }
    vec3 mrChannels = hasMetallicRoughness ? texture(metallicRoughnessTexture, uv).rgb : vec3(1);
    float metal = clamp(metallicFactor * mrChannels.b, 0.0, 1.0);
    float rough = clamp(roughnessFactor * mrChannels.g, 0.045, 1.0);
    vec3 n = surfaceNormal();
    vec3 offset = cameraPosition - worldPosition;
    vec3 v = length(offset) > 1e-8 ? normalize(offset) : n;
    float ao = hasOcclusion ? mix(1.0, texture(occlusionTexture, uv).r, occlusionStrength) : 1.0;
    // 没有IBL时环境项仅作非金属漫反射近似，不能伪造金属的环境反射。
    vec3 result = ambientLight * surface.rgb * (1.0 - metal) * ao;
    for (int i = 0; i < directionalLightCount; ++i)
    {
        result += pbrDirect(surface.rgb, metal, rough, n, v, -directionalLights[i].direction,
            directionalLights[i].color * directionalLights[i].intensity) *
            (i == 0 ? mainLightVisibility(worldPosition,n,-directionalLights[i].direction) : 1.0);
    }
    for (int i = 0; i < pointLightCount; ++i)
    {
        vec3 toLight = pointLights[i].position - worldPosition;
        float distanceToLight = length(toLight);
        if (distanceToLight < 1e-6) { continue; }
        result += pbrDirect(surface.rgb, metal, rough, n, v, toLight / distanceToLight,
            pointLights[i].color * pointLights[i].intensity * distanceAttenuation(distanceToLight, pointLights[i].range));
    }
    for (int i = 0; i < spotLightCount; ++i)
    {
        vec3 toLight = spotLights[i].position - worldPosition;
        float distanceToLight = length(toLight);
        if (distanceToLight < 1e-6) { continue; }
        toLight /= distanceToLight;
        float cone = smoothstep(spotLights[i].outerCos, spotLights[i].innerCos, dot(-toLight, spotLights[i].direction));
        result += pbrDirect(surface.rgb, metal, rough, n, v, toLight, spotLights[i].color *
            spotLights[i].intensity * cone * distanceAttenuation(distanceToLight, spotLights[i].range));
    }
    vec3 emission = emissionFactor * (hasEmission ? texture(emissionTexture, uv).rgb : vec3(1));
    // 这里只输出线性HDR，既不Gamma编码也不裁到[0,1]；透明混合必须先于最终输出。
    FragColor = vec4(result + emission, alpha);
}
