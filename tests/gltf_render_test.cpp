#include "TestSupport.h"
#include <GL/glew.h>
#include "platform/Window.h"
#include "resources/ResourceManager.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "scene/ModelInstantiator.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/camera/Camera.h"
#include <array>
#include <cmath>
#include <iostream>

namespace
{
    // 使用sRGB离屏目标并在绘制前启用自动编码，验证预览Shader不会被重复Gamma编码。
    // 固定尺寸也隔离了桌面缩放、窗口颜色格式和交换缓冲的影响。
    struct Target
    {
        GLuint fbo = 0, color = 0, depth = 0;
        Target()
        {
            glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glGenRenderbuffers(1, &color); glBindRenderbuffer(GL_RENDERBUFFER, color);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, 64, 64);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
            glGenRenderbuffers(1, &depth); glBindRenderbuffer(GL_RENDERBUFFER, depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
            require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Incomplete framebuffer");
            glViewport(0, 0, 64, 64);
        }
        ~Target()
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0); glDeleteFramebuffers(1, &fbo);
            glDeleteRenderbuffers(1, &color); glDeleteRenderbuffers(1, &depth);
        }
    };

    void pixel(int x, int y, std::array<int, 3> expected)
    {
        std::array<unsigned char, 4> actual{};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
        for (int i = 0; i < 3; ++i)
        { require(std::abs(int(actual[i]) - expected[i]) <= 3, "Unexpected pixel at " + std::to_string(x) + "," + std::to_string(y)); }
    }
}

int main(int argc, char **argv)
{
    try
    {
        require(argc == 2, "Expected test output directory");
        Window window(64, 64, "glTF Render Test", false);
        Target target;
        ResourceManager resources;
        Renderer renderer;
        Camera camera; camera.setOrthographic(2, .1f, 100);
        const auto vert = "examples/gltf_model/shaders/preview.vert";
        const auto frag = "examples/gltf_model/shaders/preview.frag";
        Scene scene;
        auto model = resources.loadModel("tests/fixtures/gltf/quad.glb", vert, frag);
        auto instance = ModelInstantiator::instantiate(scene, *model);
        glEnable(GL_FRAMEBUFFER_SRGB);
        renderer.clear(0,0,0,1); scene.render(renderer, camera, 1);
        // 左上红、右上绿、左下蓝、右下黄。非对称图片能发现上下颠倒和重复翻转。
        pixel(22,42,{255,0,0}); pixel(42,42,{0,255,0});
        pixel(22,22,{0,0,255}); pixel(42,22,{255,255,0});
        require(glIsEnabled(GL_FRAMEBUFFER_SRGB), "Renderer failed to restore sRGB state");
        scene.findObject(instance.rootId)->transform.scale.x = -1;
        renderer.clear(0,0,0,1); scene.render(renderer, camera, 1);
        pixel(22,42,{0,255,0}); pixel(42,42,{255,0,0});
        ModelInstantiator::remove(scene, instance);
        auto factor = resources.loadModel("tests/fixtures/gltf/factor.gltf", vert, frag);
        instance = ModelInstantiator::instantiate(scene, *factor);
        renderer.clear(0,0,0,1); scene.render(renderer, camera, 1);
        // 128的sRGB纹理解码成约0.216，再乘0.5并编码，结果约92；直接乘会错误得到64。
        pixel(32,32,{92,92,92});
        const auto directory = std::filesystem::path(argv[1]);
        std::filesystem::create_directories(directory);
        expectThrow<std::invalid_argument>([&] { SceneSerializer::save(scene, directory / "unsupported.scene"); },
            "glTF primitive silently serialized as OBJ");
        require(glGetError() == GL_NO_ERROR, "glTF drawing left GL errors");
        std::cout << "glTF UV/sRGB/mirrored culling pixels passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
