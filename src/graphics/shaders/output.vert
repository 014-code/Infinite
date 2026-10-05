#version 330 core
out vec2 uv;
void main()
{
    // 无顶点缓冲的全屏大三角形，消除两个三角形对角线边界。
    vec2 point = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = point;
    gl_Position = vec4(point * 2.0 - 1.0, 0, 1);
}
