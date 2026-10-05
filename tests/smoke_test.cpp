#include "graphics/camera/Camera.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Material.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"
#include "math/Transform.h"
#include "platform/Window.h"

#include <GL/glew.h>

#include <iostream>
#include <array>
#include <vector>

namespace
{
    // 测试自己的最小矩形数据，不依赖任何示例工厂函数。
    Mesh createSmokeQuad()
    {
        const std::vector<float> vertices = {
            -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f
        };

        return Mesh(vertices);
    }
}

int main()
{
    try
    {
        // 创建隐藏窗口，为OpenGL资源提供有效上下文
        Window window(320, 240, "Infinite Smoke Test", false);
        Shader shader(
            "examples/textured_quad/shaders/texture.vert",
            "examples/textured_quad/shaders/texture.frag");
        Mesh quad = createSmokeQuad();
        Texture texture("examples/textured_quad/assets/checker.ppm");
        Material material(shader, glm::vec4(1.0f), &texture);
        Transform transform;
        Camera camera;
        Renderer renderer;

        // 执行一次完整的清屏、绑定资源和绘制流程
        renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);
        renderer.draw(
            quad,
            material,
            transform,
            camera,
            window.aspectRatio());
        glFinish();

        // 读取窗口中心像素，确认纹理绘制结果不是清屏颜色。
        std::array<unsigned char, 4> centerPixel{};
        glReadPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centerPixel.data());
        if (centerPixel[0] == 0 && centerPixel[1] == 0 && centerPixel[2] == 0)
        {
            std::cerr << "OpenGL smoke test failed: textured pixel was not rendered" << std::endl;
            return 1;
        }

        const GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cerr << "OpenGL smoke test failed with error code: " << error << std::endl;
            return 1;
        }

        std::cout << "OpenGL smoke test passed" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "OpenGL smoke test failed: " << exception.what() << std::endl;
        return 1;
    }
}
