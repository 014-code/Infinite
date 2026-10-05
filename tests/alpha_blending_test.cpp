#include "support/ColorDepthTarget.h"
#include "TestSupport.h"
#include "graphics/camera/Camera.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"
#include "math/Transform.h"
#include "platform/Window.h"

// 排序现已迁入框架渲染层，示例与测试复用同一套实现。
#include "graphics/rendering/RenderItem.h"

#include <glm/gtc/matrix_transform.hpp>

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

    Mesh createQuad()
    {
        // 白色矩形覆盖窗口中心，避免边缘插值影响混合结果。
        return Mesh({
            -0.8f, -0.8f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
             0.8f, -0.8f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
             0.8f,  0.8f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
             0.8f,  0.8f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            -0.8f,  0.8f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
            -0.8f, -0.8f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f
        });
    }

    std::array<unsigned char, 4> centerPixel()
    {
        std::array<unsigned char, 4> pixel{};
        // glReadPixels等待对应绘制完成，不需要固定sleep或额外glFinish。
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        return pixel;
    }

    float centerDepth()
    {
        float depth = 0.0f;
        glReadPixels(32, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        return depth;
    }

    void expectColor(const std::array<int, 4> &expected, const char *label)
    {
        const auto actual = centerPixel();
        for (size_t channel = 0; channel < actual.size(); ++channel)
        {
            // RGBA8存储和混合存在量化取整，容许2级误差而非浮点完全相等。
            require(std::abs(static_cast<int>(actual[channel]) - expected[channel]) <= 2,
                std::string(label) + ": channel " + std::to_string(channel)
                + ", actual=" + std::to_string(actual[channel])
                + ", expected=" + std::to_string(expected[channel]));
        }
        require(glGetError() == GL_NO_ERROR, std::string("OpenGL error: ") + label);
    }
}

int main()
{
    try
    {
        Window window(64, 64, "Infinite Alpha Blending Test", false);
        ColorDepthTarget framebuffer;
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
            "Incomplete alpha test framebuffer");
        glDisable(GL_DITHER);
        glDisable(GL_FRAMEBUFFER_SRGB);

        Shader colorShader("examples/alpha_blending/shaders/color.vert",
            "examples/alpha_blending/shaders/color.frag");
        Shader textureShader("examples/alpha_blending/shaders/texture.vert",
            "examples/alpha_blending/shaders/texture.frag");
        Mesh quad = createQuad();
        Material background(colorShader, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        Material red(colorShader, glm::vec4(1.0f, 0.0f, 0.0f, 0.5f));
        Material green(colorShader, glm::vec4(0.0f, 1.0f, 0.0f, 0.5f));
        Transform backgroundTransform;
        backgroundTransform.position.z = -0.8f; // 底板比两张透明矩形都远。
        Transform nearTransform;
        nearTransform.position.z = 0.2f;
        Transform farTransform;
        farTransform.position.z = -0.2f;
        Camera camera;
        Renderer renderer;
        renderer.setDepthTestEnabled(true);
        renderer.setFaceCullingEnabled(false);

        const auto draw = [&](const Material &material, const Transform &transform)
        {
            renderer.draw(quad, material, transform, camera, 1.0f);
        };
        const auto beginOnBlue = [&]()
        {
            renderer.setAlphaBlendingEnabled(false);
            renderer.setDepthWriteEnabled(true);
            renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);
            draw(background, backgroundTransform);
            renderer.setAlphaBlendingEnabled(true);
            renderer.setDepthWriteEnabled(false);
        };

        // 故意设置错误的外部混合参数，再检查API每次启用都重设完整约定。
        renderer.setDepthWriteEnabled(false);
        glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT);
        glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ONE);
        renderer.setAlphaBlendingEnabled(true);
        require(glIsEnabled(GL_BLEND) && state(GL_BLEND_EQUATION_RGB) == GL_FUNC_ADD &&
            state(GL_BLEND_EQUATION_ALPHA) == GL_FUNC_ADD && state(GL_BLEND_SRC_RGB) == GL_SRC_ALPHA &&
            state(GL_BLEND_DST_RGB) == GL_ONE_MINUS_SRC_ALPHA && state(GL_BLEND_SRC_ALPHA) == GL_ONE &&
            state(GL_BLEND_DST_ALPHA) == GL_ONE_MINUS_SRC_ALPHA, "Incorrect blend state");
        require(glIsEnabled(GL_DEPTH_TEST) && !state(GL_DEPTH_WRITEMASK) && !glIsEnabled(GL_CULL_FACE),
            "Blending changed unrelated render states");

        // 三个独立场景验证Alpha端点和0.5的公式，互相不留下深度或颜色。
        Material invisible(colorShader, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
        Material solidRed(colorShader, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        beginOnBlue();
        draw(invisible, nearTransform);
        expectColor({0, 0, 255, 255}, "Alpha zero preserves blue");
        beginOnBlue();
        draw(solidRed, nearTransform);
        expectColor({255, 0, 0, 255}, "Alpha one covers blue");
        beginOnBlue();
        const float opaqueDepth = centerDepth();
        draw(red, nearTransform);
        expectColor({128, 0, 128, 255}, "Half red over blue");
        require(std::abs(centerDepth() - opaqueDepth) < 0.00001f, "Transparent draw wrote depth");

        // 在Alpha=0的空背景上验证输出Alpha：连续两层0.5应累积成0.75，而非0.375。
        renderer.clear(0.0f, 0.0f, 0.0f, 0.0f);
        draw(green, farTransform);
        expectColor({0, 128, 0, 128}, "First layer output alpha");
        draw(red, nearTransform);
        expectColor({128, 64, 0, 191}, "Accumulated output alpha");

        const auto renderSorted = [&](bool reverseInput)
        {
            beginOnBlue();
            std::vector<RenderItem> items = {
                {&quad, &red, &nearTransform}, {&quad, &green, &farTransform}
            };
            if (reverseInput)
            {
                std::reverse(items.begin(), items.end());
            }
            sortTransparentBackToFront(items, camera.viewMatrix());
            require(items.front().transform == &farTransform, "Sort did not choose far object first");
            for (const auto &item : items)
            {
                draw(*item.material, *item.transform);
            }
            expectColor({128, 64, 64, 255}, "Far green then near red over blue");
            return centerPixel();
        };
        const auto first = renderSorted(false);
        require(first == renderSorted(true), "Shuffling submissions changed sorted output");
        // 对照组故意顺序错误，颜色必须不同，证明排序不是一个不起作用的检查。
        beginOnBlue();
        draw(red, nearTransform);
        draw(green, farTransform);
        expectColor({64, 128, 64, 255}, "Unsorted control image");
        require(first != centerPixel(), "Test scene does not expose ordering errors");

        // 从另一侧看，远近关系应反转；排序依据视图矩阵而非硬编码的世界Z值。
        std::vector<RenderItem> oppositeItems = {
            {&quad, &green, &farTransform}, {&quad, &red, &nearTransform}
        };
        const auto oppositeView = glm::lookAt(glm::vec3(0.0f, 0.0f, -3.0f),
            glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        sortTransparentBackToFront(oppositeItems, oppositeView);
        require(oppositeItems.front().transform == &nearTransform, "Sort ignores camera direction");

        // 不透明墙先写深度，后画的透明表面位于墙后时仍被挡住。
        Material wall(colorShader, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        renderer.setAlphaBlendingEnabled(false);
        renderer.setDepthWriteEnabled(true);
        renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);
        draw(wall, nearTransform);
        renderer.setAlphaBlendingEnabled(true);
        renderer.setDepthWriteEnabled(false);
        draw(red, farTransform);
        expectColor({0, 255, 0, 255}, "Opaque wall occludes transparent object");

        // 第二帧故意从深度写入关闭状态清屏，必须清掉前一帧墙的深度并恢复写入开关。
        renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);
        require(!state(GL_DEPTH_WRITEMASK), "clear changed depth-write state");
        require(std::abs(centerDepth() - 1.0f) < 0.00001f, "clear left stale depth from previous frame");
        renderer.setAlphaBlendingEnabled(false);
        renderer.setDepthWriteEnabled(true);
        draw(solidRed, farTransform);
        expectColor({255, 0, 0, 255}, "Second-frame far object after clear");
        require(centerDepth() < 1.0f, "Opaque draw did not restore depth writing after clear");

        // 纹理Alpha乘以材质Alpha：128/255 * 0.5，红色覆盖蓝色约为25%。
        ImageData image;
        image.width = 1;
        image.height = 1;
        image.pixels = {255, 255, 255, 128};
        Texture texture(image);
        Material textured(textureShader, glm::vec4(1.0f, 0.0f, 0.0f, 0.5f), &texture);
        beginOnBlue();
        draw(textured, nearTransform);
        expectColor({64, 0, 191, 255}, "Texture alpha times material alpha");

        // 结束透明阶段：恢复写入并关闭混合，下一次绘制重新具有不透明覆盖行为。
        renderer.setDepthWriteEnabled(true);
        renderer.setAlphaBlendingEnabled(false);
        require(state(GL_DEPTH_WRITEMASK) && !glIsEnabled(GL_BLEND) && glIsEnabled(GL_DEPTH_TEST),
            "Pass did not restore expected render state");
        renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);
        draw(red, nearTransform);
        expectColor({255, 0, 0, 128}, "Disabled blending replaces color");
        require(centerDepth() < 1.0f, "Depth writing was not restored");
        renderer.setAlphaBlendingEnabled(true);
        renderer.setDepthWriteEnabled(false);
        renderer.clear(0.0f, 0.0f, 1.0f, 1.0f);
        draw(red, nearTransform);
        expectColor({128, 0, 128, 255}, "Blending re-enabled");
        renderer.setDepthWriteEnabled(true);
        renderer.setAlphaBlendingEnabled(false);

        std::cout << "Alpha blending test passed: RGBA formulas, ordering, camera direction, "
                  << "occlusion, depth clearing and state restoration" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Alpha blending test failed: " << exception.what() << std::endl;
        return 1;
    }
}
