#include "graphics/camera/Camera.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Shader.h"
#include "math/Transform.h"
#include "platform/Window.h"

#include <GL/glew.h>
#include <glm/vec4.hpp>

#include <array>
#include <iostream>
#include <vector>

namespace
{
    // 测试使用一个覆盖窗口中心的矩形，验证近处片段能遮挡远处片段。
    Mesh createDepthTestQuad()
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
        // 创建隐藏窗口，为OpenGL资源提供有效上下文。
        Window window(320, 240, "Infinite Depth Smoke Test", false);

        Shader shader(
            "examples/color_triangle/shaders/color.vert",
            "examples/color_triangle/shaders/color.frag");
        Mesh quad = createDepthTestQuad();
        Material nearMaterial(shader, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        Material farMaterial(shader, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        Transform nearTransform;
        Transform farTransform;
        // 摄像机位于Z=3，Z=0的物体比Z=-0.5的物体更近。
        farTransform.position.z = -0.5f;
        Camera camera;
        Renderer renderer;
        renderer.setDepthTestEnabled(true);

        renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);

        // 先绘制近处红色矩形，再绘制远处蓝色矩形。
        renderer.draw(
            quad,
            nearMaterial,
            nearTransform,
            camera,
            window.aspectRatio());
        renderer.draw(
            quad,
            farMaterial,
            farTransform,
            camera,
            window.aspectRatio());
        glFinish();

        std::array<unsigned char, 4> centerPixel{};
        glReadPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centerPixel.data());
        if (centerPixel[0] <= centerPixel[2])
        {
            std::cerr << "Depth smoke test failed: far object covered near object" << std::endl;
            return 1;
        }

        const GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cerr << "Depth smoke test failed with error code: " << error << std::endl;
            return 1;
        }

        std::cout << "Depth smoke test passed" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Depth smoke test failed: " << exception.what() << std::endl;
        return 1;
    }
}
