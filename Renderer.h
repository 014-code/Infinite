#pragma once

class Mesh;
class Shader;

class Renderer
{
public:
    // 清空当前帧缓冲区
    void clear(float red, float green, float blue, float alpha) const;

    // 使用着色器程序绘制网格
    void draw(const Shader &shader, const Mesh &mesh) const;
};
