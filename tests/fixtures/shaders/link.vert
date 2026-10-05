#version 330 core

// 同一个程序中，两个阶段的同名uniform类型必须一致。
// 两份文件单独编译合法，但链接时不能合并float和vec4。
uniform float sharedValue;
void main()
{
    gl_Position = vec4(sharedValue, 0.0, 0.0, 1.0);
}
