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
    float diffuse = max(dot(normalize(worldNormal), -lightDirection), 0.0);
    vec3 illumination = ambientLight + lightColor * lightIntensity * diffuse;
    FragColor = vec4(baseColor.rgb * vertexColor * illumination, baseColor.a);
}
