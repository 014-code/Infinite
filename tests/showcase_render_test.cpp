#include "TestSupport.h"
#include "material_showcase/ShowcaseScene.h"
#include "graphics/rendering/OpenGLDebug.h"
#include "graphics/geometry/Mesh.h"
#include "support/ColorDepthTarget.h"

#include <GL/glew.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>
#include <cmath>

namespace
{
    void verifyPlatform(Application &app, ObjectId platformId)
    {
        const auto *platform = app.scene().findObject(platformId);
        require(platform != nullptr && platform->mesh() != nullptr, "Missing showcase platform");
        const auto &bounds = platform->mesh()->bounds();
        require(std::abs(bounds.minimum.y) < 0.0001f && std::abs(bounds.maximum.y) < 0.0001f,
            "Showcase platform is not an XZ plane");
        require(std::abs(bounds.maximum.x - bounds.minimum.x - 8.4f) < 0.0001f &&
            std::abs(bounds.maximum.z - bounds.minimum.z - 4.4f) < 0.0001f &&
            std::abs(platform->transform.position.y + 1.15f) < 0.0001f,
            "Showcase platform dimensions/height changed");

        // 单独绘制平台，避免模型像素掩盖平台缺失；从上下两侧验证实际绕序和剔除。
        ColorDepthTarget target;
        Renderer renderer;
        Camera camera;
        camera.setOrthographic(10.0f, .1f, 30.0f);
        const glm::vec3 center(0, -1.15f, 0);
        for (const float side : {1.0f, -1.0f})
        {
            camera.setView(center + glm::vec3(0, side * 10.0f, 0), center, {0, 0, -1});
            renderer.clear(0, 0, 0, 1);
            renderer.drawItems({{platform->mesh(), platform->material(), &platform->transform}},
                camera, 1.0f, app.scene().lighting());
            std::vector<float> depth(64 * 64);
            glReadPixels(0, 0, 64, 64, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
            const auto visible = std::count_if(depth.begin(), depth.end(), [](float value) { return value < 1.0f; });
            require(side > 0 ? visible > 1000 : visible == 0,
                "Platform must be visible from above and culled from below");
        }
    }
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected output directory");
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        ApplicationConfig config;
        config.visible = false;
        config.clearColor = {0.12f, 0.14f, 0.16f, 1};
        Application application(config);
        MaterialShowcase::Exhibits exhibits;
        ApplicationCallbacks callbacks;
        callbacks.initialize = [&](Application &app)
        {
            exhibits = MaterialShowcase::createScene(app, "examples/material_showcase");
        };
        callbacks.afterRender = [&](Application &app)
        {
            // 每个根对象代表一个完整glTF实例；节点和primitive对象由实例化器展开到Scene。
            for (const auto &instance : exhibits.instances)
            {
                require(instance.rootId != 0 && instance.objectIds.size() > 1,
                    "glTF model instance was not expanded");
                require(app.scene().findObject(instance.rootId) != nullptr,
                    "glTF model root is missing");
            }
            require(app.scene().objectCount() > 10, "Showcase did not create model nodes and primitives");

            const auto size = app.window().framebufferSize();
            const auto count = static_cast<std::size_t>(size.x) * size.y;
            std::vector<float> depth(count);
            glReadPixels(0, 0, size.x, size.y, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
            const auto visible = std::count_if(depth.begin(), depth.end(), [](float value)
            {
                return value < 0.99999f;
            });
            require(visible > 1000, "Model gallery did not render visible geometry");

            std::vector<unsigned char> pixels(count * 4), topDown(count * 4);
            glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            // OpenGL从下往上读，PNG从上往下写；这里只翻转截图，不改变模型UV约定。
            const auto row = static_cast<std::size_t>(size.x) * 4;
            for (int y = 0; y < size.y; ++y)
            {
                std::copy_n(pixels.data() + (size.y - 1 - y) * row, row, topDown.data() + y * row);
            }
            const auto screenshot = (output / "material-showcase.png").string();
            require(stbi_write_png(screenshot.c_str(), size.x, size.y, 4, topDown.data(), size.x * 4) != 0,
                "Failed to write showcase screenshot");
            verifyPlatform(app, exhibits.platformId);
            require(OpenGLDebug::checkErrors("showcase render", __FILE__, __LINE__), "OpenGL error");
            app.requestClose();
        };
        application.run(callbacks);
        std::cout << "Three downloaded glTF exhibits rendered and screenshot saved\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
