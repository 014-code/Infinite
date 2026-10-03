#include "Renderer.h"

#include "Mesh.h"
#include "Shader.h"

void Renderer::clear(float red, float green, float blue, float alpha) const
{
    // 设置清屏颜色
    glClearColor(red, green, blue, alpha);
    // 清空颜色缓冲区
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::draw(const Shader &shader, const Mesh &mesh) const
{
    // 使用着色器程序并绘制三角形
    shader.use();
    mesh.draw();
}
