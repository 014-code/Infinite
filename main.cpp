#include "Mesh.h"
#include "Renderer.h"
#include "Shader.h"
#include "Window.h"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        // 创建窗口和OpenGL上下文
        Window window(1280, 800, "OpenGL Window");

        // 加载并链接着色器程序
        Shader shader("shaders/triangle.vert", "shaders/triangle.frag");

        // 创建带有顶点颜色的三角形网格
        Mesh triangle = createColorTriangle();
        Renderer renderer;

        // 窗口主循环
        while (!window.shouldClose())
        {
            // 渲染
            renderer.clear(0.2f, 0.1f, 0.15f, 1.0f);
            renderer.draw(shader, triangle);

            // 交换缓冲区
            window.swapBuffers();
            // 处理事件
            window.pollEvents();
        }
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << std::endl;
        return -1;
    }

    return 0;
}
