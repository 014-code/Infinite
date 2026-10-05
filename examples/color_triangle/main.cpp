#include "../common/ExampleRun.h"

#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Shader.h"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <vector>

namespace
{
    // 示例自己的几何数据，框架层只负责接收和绘制Mesh。
    Mesh createColorTriangle()
    {
        // 每个顶点依次包含位置XYZ、颜色RGB和纹理坐标UV。
        // 使用Vertex分组后，不再需要手工数连续float的偏移。
        const std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 左下角，红色
            {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // 右下角，绿色
            {{ 0.0f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}}  // 顶部，蓝色
        };
        return Mesh(vertices);
    }
}

int main(int argc, char *argv[])
{
    return runExample(argc, argv, "color_triangle", "Infinite - Color Triangle Example",
        {0.2f, 0.1f, 0.15f, 1.0f}, [](Application &application, const std::filesystem::path &directory)
    {
        // 窗口和OpenGL上下文已经创建；从示例exe旁边加载示例自己的Shader。
        // Shader由应用级资源管理器缓存，重复使用同一文件时不会重新编译GPU程序。
        auto shader = application.resources().loadShader(
            directory / "shaders/color.vert", directory / "shaders/color.frag");
        auto material = std::make_shared<Material>(shader);
        // 创建示例几何体，Mesh本身仍然来自框架层。
        auto triangle = std::make_shared<Mesh>(createColorTriangle());
        auto &object = application.scene().createObject("triangle");
        // 场景共享持有资源；初始化回调返回后，局部shared_ptr销毁也不影响物体绘制。
        object.setRenderable(triangle, material);
        const auto &input = application.input();
        object.setUpdateCallback([&input](GameObject &self, float deltaTime)
        {
            // 根据按键和帧间隔移动三角形。归一化后斜向移动不会比直线移动更快。
            const float movementSpeed = 1.0f;
            glm::vec2 movementDirection(0.0f);
            if (input.isKeyDown(Key::W))
            {
                movementDirection.y += 1.0f;
            }
            if (input.isKeyDown(Key::S))
            {
                movementDirection.y -= 1.0f;
            }
            if (input.isKeyDown(Key::A))
            {
                movementDirection.x -= 1.0f;
            }
            if (input.isKeyDown(Key::D))
            {
                movementDirection.x += 1.0f;
            }
            if (glm::length(movementDirection) > 0.0f)
            {
                movementDirection = glm::normalize(movementDirection) * movementSpeed * deltaTime;
                self.transform.position += glm::vec3(movementDirection, 0.0f);
            }
        });
        // 示例主循环交给Application：先处理事件，再更新物体、清屏并绘制彩色三角形。
    });
}
