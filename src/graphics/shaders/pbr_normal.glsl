// 先用模型提供的切线；未提供时从当前三角形的位置/UV导数重建TBN。
// 导数方法不是MikkTSpace烘焙切线的完整替代：精确匹配烘焙图应导出TANGENT。
vec3 surfaceNormal()
{
    vec3 n = length(worldNormal) > 1e-8 ? normalize(worldNormal) : vec3(0, 0, 1);
    if (!gl_FrontFacing) { n = -n; }
    if (!hasNormalTexture) { return n; }
    vec3 t, b;
    if (abs(worldTangent.w) > 0.5)
    {
        t = worldTangent.xyz - n * dot(n, worldTangent.xyz);
        if (length(t) < 1e-8) { return n; }
        t = normalize(t);
        b = cross(n, t) * sign(worldTangent.w);
    }
    else
    {
        vec3 px = dFdx(worldPosition), py = dFdy(worldPosition);
        vec2 tx = dFdx(uv), ty = dFdy(uv);
        float determinantUV = tx.x * ty.y - tx.y * ty.x;
        // 退化UV没有可定义的切线空间，保留几何法线，不能normalize(0)污染整帧。
        if (abs(determinantUV) < 1e-10) { return n; }
        t = (px * ty.y - py * tx.y) / determinantUV;
        b = (-px * ty.x + py * tx.x) / determinantUV;
        t -= n * dot(n, t);
        if (length(t) < 1e-8 || length(b) < 1e-8) { return n; }
        t = normalize(t);
        b = cross(n, t) * (dot(cross(n, t), b) < 0 ? -1.0 : 1.0);
    }
    vec3 mapped = texture(normalTexture, uv).xyz * 2.0 - 1.0;
    mapped.xy *= normalScale;
    if (length(mapped) < 1e-8) { return n; }
    return normalize(mat3(t, b, n) * mapped);
}
