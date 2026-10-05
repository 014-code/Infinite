#include "TestSupport.h"
#include "graphics/camera/Camera.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Shader.h"
#include "math/Transform.h"
#include "platform/Window.h"

// 引用的是示例数据，不把立方体工厂放回框架。这样测试能发现真实示例的绕序错误。
#include "textured_cube/CubeGeometry.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    GLint state(GLenum name)
    {
        GLint value = 0;
        glGetIntegerv(name, &value);
        return value;
    }

    Mesh createTriangle(bool clockwise)
    {
        // 从Z轴正方向观察：左下、右下、上方构成逆时针；交换后两个顶点即可反向。
        // 顶点颜色保持白色，让Material决定颜色，避免颜色插值干扰像素断言。
        std::vector<float> vertices = {
            -0.6f, -0.6f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
             0.6f, -0.6f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
             0.0f,  0.6f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 1.0f
        };
        if (clockwise)
        {
            std::swap_ranges(vertices.begin() + 8, vertices.begin() + 16, vertices.begin() + 16);
        }
        return Mesh(vertices);
    }

    void checkCubeWinding(const std::vector<Vertex> &vertices)
    {
        require(vertices.size() == 36, "Cube must contain 36 position/color/UV vertices");
        const std::array<glm::vec3, 6> outwardNormals = {{
            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
            {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}
        }};
        for (size_t triangle = 0; triangle < 12; ++triangle)
        {
            const size_t offset = triangle * 3;
            const glm::vec3 a = vertices[offset].position;
            const glm::vec3 b = vertices[offset + 1].position;
            const glm::vec3 c = vertices[offset + 2].position;
            // 右手叉积给出顶点绕序对应的方向；每个三角形都应非退化且朝向该面的外侧。
            const glm::vec3 normal = glm::cross(b - a, c - a);
            require(glm::length(normal) > 0.001f &&
                glm::dot(glm::normalize(normal), outwardNormals[triangle / 2]) > 0.999f,
                "Cube winding is incorrect at triangle " + std::to_string(triangle));
        }
    }

    std::vector<unsigned char> readFrame(int width, int height)
    {
        std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
        // 读取未交换的后缓冲，glReadPixels会等待绘制完成；无需固定等待时间。
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    }
}

int main()
{
    try
    {
        const auto cubeVertices = CubeExample::vertices();
        checkCubeWinding(cubeVertices);
        Window window(160, 160, "Infinite Culling Smoke Test", false);
        Shader shader("tests/fixtures/culling.vert", "tests/fixtures/culling.frag");
        Material material(shader, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        Mesh front = createTriangle(false);
        Mesh back = createTriangle(true);
        Mesh cube(cubeVertices);
        Transform transform;
        Camera camera;
        Renderer renderer;

        // 用实际帧缓冲尺寸处理高DPI环境，保证读取点处于三角形内部。
        std::array<GLint, 4> viewport{};
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        const int width = viewport[2];
        const int height = viewport[3];
        require(width > 0 && height > 0, "Empty framebuffer");
        glReadBuffer(GL_BACK);
        renderer.setDepthTestEnabled(true);

        // 故意留下相反的约定，检查启用接口会明确重设成GL_BACK和GL_CCW。
        glCullFace(GL_FRONT);
        glFrontFace(GL_CW);
        renderer.setFaceCullingEnabled(true);
        require(glIsEnabled(GL_CULL_FACE) == GL_TRUE && state(GL_CULL_FACE_MODE) == GL_BACK &&
            state(GL_FRONT_FACE) == GL_CCW, "Culling state was not configured correctly");
        require(glIsEnabled(GL_DEPTH_TEST) == GL_TRUE, "Culling changed depth test state");

        const auto checkTriangle = [&](const Mesh &mesh, bool visible, const char *label)
        {
            renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);
            renderer.draw(mesh, material, transform, camera, window.aspectRatio());
            std::array<unsigned char, 4> pixel{};
            glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            const int expectedRed = visible ? 255 : 0;
            require(std::abs(static_cast<int>(pixel[0]) - expectedRed) <= 2 &&
                pixel[1] <= 2 && pixel[2] <= 2, std::string("Unexpected center pixel: ") + label);
            require(glGetError() == GL_NO_ERROR, std::string("OpenGL error: ") + label);
        };

        checkTriangle(front, true, "CCW front face");
        checkTriangle(back, false, "CW back face culled");
        renderer.setFaceCullingEnabled(false);
        require(glIsEnabled(GL_CULL_FACE) == GL_FALSE && glIsEnabled(GL_DEPTH_TEST) == GL_TRUE,
            "Disabling culling changed unrelated state");
        checkTriangle(back, true, "CW back face visible after disabling culling");
        renderer.setFaceCullingEnabled(true);
        checkTriangle(back, false, "Culling enabled again");

        // 镜像缩放确实会翻转绕序；接口不应偷偷更改物体变换或正面约定。
        transform.scale.x = -1.0f;
        checkTriangle(front, false, "Mirrored front becomes back");
        transform.scale.x = 1.0f;

        // 验证真实立方体：深度测试始终开启，剔除前后图像应一致且非空。
        // 六个主方向加两个斜视角覆盖六个面，避免只测一个角度遗漏漏面问题。
        const float halfPi = glm::half_pi<float>();
        const std::array<glm::vec3, 8> rotations = {{
            {0.0f, 0.0f, 0.0f}, {0.0f, 2 * halfPi, 0.0f},
            {0.0f, halfPi, 0.0f}, {0.0f, -halfPi, 0.0f},
            {halfPi, 0.0f, 0.0f}, {-halfPi, 0.0f, 0.0f},
            {0.4f, 0.7f, 0.2f}, {-0.6f, -0.8f, 0.3f}
        }};
        for (const auto &rotation : rotations)
        {
            transform.setEulerAngles(rotation);
            renderer.setFaceCullingEnabled(false);
            renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);
            renderer.draw(cube, material, transform, camera, window.aspectRatio());
            const auto reference = readFrame(width, height);
            renderer.setFaceCullingEnabled(true);
            renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);
            renderer.draw(cube, material, transform, camera, window.aspectRatio());
            const auto culled = readFrame(width, height);
            size_t visiblePixels = 0;
            for (size_t offset = 0; offset < reference.size(); offset += 4)
            {
                visiblePixels += reference[offset] > 200;
                require(std::abs(static_cast<int>(reference[offset]) - culled[offset]) <= 2,
                    "Cube image changed when back-face culling was enabled");
            }
            require(visiblePixels > 100, "Cube comparison rendered an empty image");
            require(glGetError() == GL_NO_ERROR, "OpenGL error during cube comparison");
        }

        std::cout << "Culling smoke test passed: front/back, disable/re-enable, mirror, "
                  << "12 outward triangles and 8 cube views" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Culling smoke test failed: " << exception.what() << std::endl;
        return 1;
    }
}
