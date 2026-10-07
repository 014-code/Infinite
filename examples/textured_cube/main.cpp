#include "../common/ExampleRun.h"
#include "CubeGeometry.h"

#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"

#include <glm/vec3.hpp>
#include <memory>

int main(int argc, char *argv[])
{
    return runExample(argc, argv, "textured_cube", "Infinite - Textured Cube Example",
        {0.2f, 0.1f, 0.15f, 1.0f}, [](Application &application, const std::filesystem::path &directory)
    {
        // 创建窗口和OpenGL上下文后，从示例exe旁边加载Shader和纹理资源。
        // ResourceManager负责缓存文件型资源，Material仍由示例按自己的颜色组合。
        auto shader = application.resources().loadShader(
            directory / "shaders/texture.vert", directory / "shaders/texture.frag");
        auto texture = application.resources().loadTexture(directory / "assets/checker.ppm");
        auto material = std::make_shared<Material>(shader, glm::vec4(1.0f), texture);
        // 创建示例自己的立方体网格；Scene只管理对象，示例几何和外观仍由本应用提供。
        auto cube = std::make_shared<Mesh>(CubeExample::vertices());
        auto &object = application.scene().createObject("cube");
        object.setRenderable(cube, material);
        // 闭合立方体的三角形从外侧看都是逆时针，背向观察者的面可以直接剔除。
        // 深度测试仍然保留，用来解决可见片段之间的前后遮挡。
        material->setCullMode(CullMode::Back);
        // 这里使用正缩放；单轴负缩放会翻转绕序，不能直接沿用原来的正面约定。
        object.script().setUpdateCallback([](GameObject &self, float deltaTime)
        {
            // 旋转立方体，便于观察不同面的前后关系。
            // 用模拟dt累加旋转，恢复窗口时不会根据真实经过时间突然跳转角度。
            self.transform.rotateEuler(glm::vec3(deltaTime * 0.7f, deltaTime, 0.0f));
        });
        // Application的主循环在清屏时同时重置颜色和深度缓冲。
    });
}
