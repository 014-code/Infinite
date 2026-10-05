#version 330 core

in vec3 worldNormal;
in vec3 vertexColor;
uniform vec4 baseColor;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform vec3 ambientLight;
out vec4 FragColor;

void main()
{
    // direction约定为光线传播方向，取反后才是表面指向光源的方向。
    // 点积是夹角余弦；背向光源时截为0，不产生“负的光”。
    float diffuse = max(dot(normalize(worldNormal), -lightDirection), 0.0);
    vec3 illumination = ambientLight + lightColor * lightIntensity * diffuse;
    // baseColor是材质表面颜色，光源参数属于场景；不用新增一份重复的漫反射颜色。
    // 本示例直接输出线性数值到普通颜色缓冲，暂不做纹理、Gamma和高光。
    FragColor = vec4(baseColor.rgb * vertexColor * illumination, baseColor.a);
}
