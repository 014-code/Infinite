#version 330 core

uniform vec4 sharedValue;
out vec4 FragColor;
void main()
{
    FragColor = sharedValue;
}
