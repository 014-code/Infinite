#include "../common/ExampleRun.h"

#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <vector>

namespace
{
    // 示例自己的矩形几何数据，框架层只负责接收和绘制Mesh。
    Mesh createTexturedQuad()
    {
        // 每个顶点依次包含位置XYZ、颜色RGB和纹理坐标UV。
        // 两个三角形按逆时针方向排列，UV的原点位于图片左下角。
        const std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };
        // 用索引复用公共顶点；数据仍归示例所有，不在引擎里写死矩形。
        return Mesh(vertices, {0, 1, 2, 2, 3, 0});
    }
}

int main(int argc, char *argv[])
{
    return runExample(argc, argv, "textured_quad", "Infinite - Textured Quad Example",
        {0.2f, 0.1f, 0.15f, 1.0f}, [](Application &application, const std::filesystem::path &directory)
    {
        // 从示例exe旁边加载Shader和纹理资源；ResourceManager会缓存重复请求。
        auto shader = application.resources().loadShader(
            directory / "shaders/texture.vert", directory / "shaders/texture.frag");
        auto texture = application.resources().loadTexture(directory / "assets/checker.ppm");
        auto material = std::make_shared<Material>(shader, glm::vec4(1.0f), texture);
        // 创建示例几何体，Mesh和Texture都来自框架层。
        auto quad = std::make_shared<Mesh>(createTexturedQuad());
        auto &object = application.scene().createObject("quad");
        object.setRenderable(quad, material);
        object.transform.scale = glm::vec3(2.0f);
        const auto &input = application.input();
        object.script().setUpdateCallback([&input](GameObject &self, float deltaTime)
        {
            // 根据按键和帧间隔移动纹理矩形；dt已由Application处理暂停和大幅卡顿。
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
        // 清屏并绘制带纹理的矩形、交换缓冲和退出清理均由Application统一完成。
    });
}
