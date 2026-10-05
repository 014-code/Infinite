#version 330 core

// baseColor的Alpha作为材质整体透明度。
in vec3 vertexColor;
uniform vec4 baseColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vertexColor, 1.0) * baseColor;
}
