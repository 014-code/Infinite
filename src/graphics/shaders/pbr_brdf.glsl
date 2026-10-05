// Cook-Torrance：GGX法线分布、Smith联合可见性、Schlick Fresnel。
// roughness是感知粗糙度；最小值避免镜面极限在有限精度下产生除零。
const float PI = 3.141592653589793;
vec3 pbrDirect(vec3 base, float metal, float rough, vec3 n, vec3 v, vec3 l, vec3 radiance)
{
    float nl = max(dot(n, l), 0.0), nv = max(dot(n, v), 0.0);
    vec3 halfVector = v + l;
    if (nl <= 0.0 || nv <= 0.0 || dot(halfVector, halfVector) < 1e-12) { return vec3(0); }
    vec3 h = normalize(halfVector);
    float nh = max(dot(n, h), 0.0), vh = max(dot(v, h), 0.0);
    float a = max(rough * rough, 0.002025), a2 = a * a;
    float d = nh * nh * (a2 - 1.0) + 1.0;
    float distribution = a2 / (PI * d * d);
    float visibility = 0.5 / max(nl * sqrt(nv * nv * (1.0 - a2) + a2)
        + nv * sqrt(nl * nl * (1.0 - a2) + a2), 1e-6);
    vec3 fresnel = mix(vec3(0.04), base, metal);
    fresnel += (1.0 - fresnel) * pow(1.0 - vh, 5.0);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metal) * base / PI;
    return (diffuse + distribution * visibility * fresnel) * radiance * nl;
}
