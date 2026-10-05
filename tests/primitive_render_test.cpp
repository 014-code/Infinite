#include "TestSupport.h"
#include "platform/Window.h"
#include "resources/PrimitiveResources.h"
#include "scene/Scene.h"
#include "graphics/camera/Camera.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Material.h"
#include "graphics/resources/Texture.h"
#include "graphics/lighting/Lighting.h"
#include "core/Application.h"
#include "primitive_shapes/PrimitiveScene.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace
{
    // 固定离屏目标，不依赖桌面DPI，不把“窗口打开成功”误认为“所有几何体都画对了”。
    struct Target
    {
        GLuint fbo = 0, color = 0, depth = 0;
        Target()
        {
            glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glGenRenderbuffers(1, &color); glBindRenderbuffer(GL_RENDERBUFFER, color);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 64, 64);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
            glGenRenderbuffers(1, &depth); glBindRenderbuffer(GL_RENDERBUFFER, depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
            require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Incomplete primitive FBO");
            glViewport(0, 0, 64, 64);
        }
        ~Target()
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0); glDeleteFramebuffers(1, &fbo);
            glDeleteRenderbuffers(1, &color); glDeleteRenderbuffers(1, &depth);
        }
    };

    void checkPixel(int x, int y, std::array<int, 3> expected)
    {
        std::array<unsigned char, 4> actual{};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
        for (int i = 0; i < 3; ++i)
        {
            require(std::abs(int(actual[i]) - expected[i]) < 4,
                "Primitive pixel mismatch: " + std::to_string(actual[i]) + " != " + std::to_string(expected[i]));
        }
    }
}

int checkPixels()
{
    try
    {
        Window window(64, 64, "Primitive pixels", false);
        Target target;
        PrimitiveResources resources;
        Renderer renderer;
        Camera camera; camera.setOrthographic(1.5f, .1f, 100);
        camera.setView({2, 2, 3}, {0, 0, 0});
        DirectionalLight light; light.intensity = 0; light.ambient = glm::vec3(1);
        for (auto type : {PrimitiveType::Cube, PrimitiveType::Plane, PrimitiveType::Disk,
            PrimitiveType::Sphere, PrimitiveType::Cylinder, PrimitiveType::Cone})
        {
            Scene scene(resources);
            PrimitiveOptions options; options.color = {.8f, .2f, .1f, 1};
            auto &object = scene.createPrimitive(type, options);
            renderer.clear(0, 0, 0, 1); scene.render(renderer, camera, 1, light);
            checkPixel(32, 32, {204, 51, 26});
            // 负缩放必须校正绕序；只看默认物体会漏掉镜像物体被错误剔除的情况。
            object.transform.scale = {-1, .8f, 1.2f};
            renderer.clear(0, 0, 0, 1); scene.render(renderer, camera, 1, light);
            checkPixel(32, 32, {204, 51, 26});
        }
        // 四角各异的纹理能发现UV颠倒；纯色或对称棋盘可能掩盖这个错误。
        ImageData image; image.width = 2; image.height = 2;
        image.pixels = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255};
        Texture::SamplingOptions sampling; sampling.minFilter = GL_NEAREST; sampling.magFilter = GL_NEAREST;
        auto texture = std::make_shared<Texture>(image, sampling);
        auto material = resources.createMaterial(PrimitiveType::Plane, glm::vec4(1));
        material->setTexture(texture);
        PlaneOptions options; options.material = material;
        Scene scene(resources); auto &plane = scene.createPlane(options);
        camera.setView({0, 3, 0}, {0, 0, 0}, {0, 0, -1});
        renderer.clear(0, 0, 0, 1); scene.render(renderer, camera, 1, light);
        checkPixel(24, 24, {255,0,0}); checkPixel(40, 24, {0,255,0});
        checkPixel(24, 40, {0,0,255}); checkPixel(40, 40, {255,255,0});

        // 使用真正的方向光，而不是仅靠环境光，验证内置Shader确实读到了法线矩阵。
        material->setTexture(std::shared_ptr<Texture>{});
        light.ambient = glm::vec3(0); light.intensity = 1; light.color = glm::vec3(1); light.direction = {0, -1, 0};
        plane.transform.scale = {2, 1, .8f};
        renderer.clear(0,0,0,1); scene.render(renderer, camera, 1, light); checkPixel(32,32,{255,255,255});
        light.direction = {0, 1, 0};
        renderer.clear(0,0,0,1); scene.render(renderer, camera, 1, light); checkPixel(32,32,{0,0,0});

        require(glGetError() == GL_NO_ERROR, "Primitive rendering left GL errors");
        std::cout << "Six primitive pixels, mirrored culling, UV and lighting passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}

int main(int argc, char **argv)
{
    try
    {
        require(argc == 2, "Expected primitive screenshot directory");
        require(checkPixels() == 0, "Isolated primitive pixels failed");
        // 上一个测试的Window和GPU资源已全部销毁；再启动真实示例场景，不交叉使用上下文。
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        ApplicationConfig config; config.visible = false;
        Application application(config);
        ApplicationCallbacks callbacks;
        callbacks.initialize = [](Application &app) { PrimitiveExample::createScene(app); };
        callbacks.afterRender = [&](Application &app)
        {
            require(app.scene().objectCount() == 6, "Primitive gallery object count changed");
            const auto size = app.window().framebufferSize();
            const auto count = static_cast<std::size_t>(size.x) * size.y;
            std::vector<float> depth(count);
            glReadPixels(0,0,size.x,size.y,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
            require(std::count_if(depth.begin(), depth.end(), [](float v) { return v < .99999f; }) > 1000,
                "Primitive gallery did not draw geometry");
            std::vector<unsigned char> pixels(count * 4), topDown(count * 4);
            glReadPixels(0,0,size.x,size.y,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            const auto rowSize = static_cast<std::size_t>(size.x) * 4;
            for (int y = 0; y < size.y; ++y)
            {
                // 仅翻转截图行顺序，不修改网格UV或纹理。
                std::copy_n(pixels.data() + (size.y - 1 - y) * rowSize, rowSize, topDown.data() + y * rowSize);
            }
            require(stbi_write_png((output / "primitive-shapes.png").string().c_str(), size.x, size.y,
                4, topDown.data(), size.x * 4) != 0, "Cannot write primitive screenshot");
            require(glGetError() == GL_NO_ERROR, "Gallery OpenGL error");
            app.requestClose();
        };
        application.run(callbacks);
        std::cout << "Primitive gallery rendered and screenshot saved\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
