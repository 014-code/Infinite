#include "../common/ExampleRun.h"
#include "../common/FreeCameraController.h"

#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"

// 立方体几何仍在示例层共享，不把特定形状放进框架。
#include "textured_cube/CubeGeometry.h"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <memory>

namespace
{
    Mesh createQuad()
    {
        // 双面图片的几何数据只属于这个示例。默认白色顶点不改变材质的颜色。
        const std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0}, {1, 1, 1}, {0, 0}},
            {{ 0.5f, -0.5f, 0}, {1, 1, 1}, {1, 0}},
            {{ 0.5f,  0.5f, 0}, {1, 1, 1}, {1, 1}},
            {{-0.5f,  0.5f, 0}, {1, 1, 1}, {0, 1}}
        };
        // 四个顶点通过六个索引组成两个三角形，不再重复保存公共顶点。
        return Mesh(vertices, {0, 1, 2, 2, 3, 0});
    }

    ImageData checkerImage()
    {
        // 程序生成的小型棋盘纹理，示例无需额外下载图片。
        ImageData image;
        image.width = 2;
        image.height = 2;
        image.pixels = {255, 255, 255, 255, 75, 100, 140, 255,
                        75, 100, 140, 255, 255, 255, 255, 255};
        return image;
    }

    ImageData whiteImage()
    {
        ImageData image;
        image.width = 1;
        image.height = 1;
        image.pixels = {255, 255, 255, 255};
        return image;
    }
}

int main(int argc, char *argv[])
{
    FreeCameraController cameraController;
    return runExample(argc, argv, "scene_objects", "Infinite - Scene Objects (WASD camera / RMB look / R reset)",
        {0.035f, 0.045f, 0.07f, 1.0f}, [&cameraController](Application &application, const std::filesystem::path &directory)
    {
        // GPU资源在有效上下文内创建，Scene持有它们直到正常退出或异常清理。
        // 场景中的多个Material共享同一个Shader；由ResourceManager统一缓存和持有。
        auto shader = application.resources().loadShader(
            directory / "shaders/scene.vert", directory / "shaders/scene.frag");
        auto cube = std::make_shared<Mesh>(CubeExample::vertices());
        auto quad = std::make_shared<Mesh>(createQuad());
        // 这两张图片是示例运行时生成的ImageData，没有磁盘路径，所以仍直接上传。
        auto checker = std::make_shared<Texture>(checkerImage());
        auto white = std::make_shared<Texture>(whiteImage());

        // 两个立方体共享Mesh和Texture，但各有材质颜色和独立Transform。
        auto warm = std::make_shared<Material>(shader, glm::vec4(1.0f, 0.55f, 0.3f, 1.0f), checker);
        auto cool = std::make_shared<Material>(shader, glm::vec4(0.35f, 0.7f, 1.0f, 1.0f), checker);
        warm->setCullMode(CullMode::Back);
        cool->setCullMode(CullMode::Back);
        auto background = std::make_shared<Material>(shader, glm::vec4(0.12f, 0.15f, 0.22f, 1.0f), white);
        auto redGlass = std::make_shared<Material>(shader, glm::vec4(1.0f, 0.2f, 0.15f, 0.4f), white);
        auto blueGlass = std::make_shared<Material>(shader, glm::vec4(0.1f, 0.6f, 1.0f, 0.4f), white);
        redGlass->setRenderMode(RenderMode::AlphaBlend);
        blueGlass->setRenderMode(RenderMode::AlphaBlend);

        // GameObject共享持有Mesh和Material，Material继续持有Shader和Texture。
        // Application先清空Scene、最后关闭Window；不要把GPU资源保存在更长寿的外部变量。
        auto &scene = application.scene();
        const auto &input = application.input();
        auto &left = scene.createObject("暖色立方体");
        left.setRenderable(cube, warm);
        left.transform.position = glm::vec3(-0.65f, 0.2f, 0.0f);
        left.transform.scale = glm::vec3(0.65f);
        auto &right = scene.createObject("冷色立方体");
        right.setRenderable(cube, cool);
        right.transform.position = glm::vec3(0.65f, 0.2f, 0.0f);
        right.transform.scale = glm::vec3(0.65f);

        // 故意先提交近处透明物体，Scene/Renderer负责分组和排序，应用不用管绘制顺序。
        auto &nearGlass = scene.createObject("近处蓝色玻璃");
        nearGlass.setRenderable(quad, blueGlass);
        nearGlass.transform.position = glm::vec3(0.18f, -0.25f, 0.75f);
        nearGlass.transform.scale = glm::vec3(0.95f, 0.85f, 1.0f);
        auto &farGlass = scene.createObject("远处红色玻璃");
        farGlass.setRenderable(quad, redGlass);
        farGlass.transform.position = glm::vec3(-0.18f, -0.25f, 0.5f);
        farGlass.transform.scale = glm::vec3(0.95f, 0.85f, 1.0f);
        auto &back = scene.createObject("不透明底板");
        back.setRenderable(quad, background);
        back.transform.position.z = -0.8f;
        back.transform.scale = glm::vec3(3.4f, 2.2f, 1.0f);

        // 不透明前景条会遮挡后面的玻璃，即使透明组在它之后绘制。
        auto &bar = scene.createObject("前景遮挡条");
        bar.setRenderable(quad, warm);
        bar.transform.position = glm::vec3(0.4f, -0.3f, 1.0f);
        bar.transform.scale = glm::vec3(0.1f, 0.85f, 1.0f);

        // 回调只捕获存活更久的Input；回调内不增删对象，active同时控制更新与绘制。
        // WASD交给摄像机，方向键继续移动暖色立方体，两个控制不会抢同一组按键。
        left.setUpdateCallback([&input](GameObject &object, float deltaTime)
        {
            // 具体运动属于应用逻辑；Scene没有内置“移动立方体”的特殊行为。
            glm::vec2 direction(0.0f);
            if (input.isKeyDown(Key::Up))
            {
                direction.y += 1.0f;
            }
            if (input.isKeyDown(Key::Down))
            {
                direction.y -= 1.0f;
            }
            if (input.isKeyDown(Key::Left))
            {
                direction.x -= 1.0f;
            }
            if (input.isKeyDown(Key::Right))
            {
                direction.x += 1.0f;
            }
            if (glm::length(direction) > 0.0f)
            {
                direction = glm::normalize(direction) * deltaTime;
                object.transform.position += glm::vec3(direction, 0.0f);
            }
            object.transform.rotateEuler(glm::vec3(deltaTime * 0.5f, deltaTime, 0.0f));
        });
        right.setUpdateCallback([](GameObject &object, float deltaTime)
        {
            object.transform.rotateEuler(glm::vec3(deltaTime * 0.7f, -deltaTime, 0.0f));
        });
        // Application在此初始化回调返回后才开始计时，首帧不包含Shader编译等耗时。
        auto &camera = application.camera();
        camera.setView({0, 0, 3}, {0, 0, 0});
        camera.setPerspective(45, 0.1f, 100);
        cameraController.apply(camera);
        // 更新由Application调用Scene统一调度，运动细节仍注册在示例回调里。
    }, ExampleFrameContent::DrawnScene,
    [&cameraController](Application &application, float deltaTime)
    {
        // 摄像机控制器属于示例层；Application只负责在Scene更新前调用应用逻辑。
        cameraController.update(application.camera(), application.input(), deltaTime, application.window().isFocused());
    });
}
