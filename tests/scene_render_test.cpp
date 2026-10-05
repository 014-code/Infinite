#include "support/ColorDepthTarget.h"
#include "TestSupport.h"
#include "graphics/camera/Camera.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"
#include "platform/Window.h"
#include "scene/Scene.h"
#include "textured_cube/CubeGeometry.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    Mesh createQuad()
    {
        return Mesh({
            -0.8f, -0.8f, 0, 1, 1, 1, 0, 0,
             0.8f, -0.8f, 0, 1, 1, 1, 1, 0,
             0.8f,  0.8f, 0, 1, 1, 1, 1, 1,
             0.8f,  0.8f, 0, 1, 1, 1, 1, 1,
            -0.8f,  0.8f, 0, 1, 1, 1, 0, 1,
            -0.8f, -0.8f, 0, 1, 1, 1, 0, 0
        });
    }

    GLint state(GLenum name)
    {
        GLint value = 0;
        glGetIntegerv(name, &value);
        return value;
    }

    std::vector<GLint> snapshot()
    {
        std::vector<GLint> values;
        for (GLenum name : {GL_DEPTH_TEST, GL_DEPTH_WRITEMASK, GL_DEPTH_FUNC, GL_BLEND,
            GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA, GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB,
            GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA, GL_CULL_FACE, GL_CULL_FACE_MODE, GL_FRONT_FACE,
            GL_CURRENT_PROGRAM, GL_VERTEX_ARRAY_BINDING, GL_ACTIVE_TEXTURE, GL_TEXTURE_BINDING_2D,
            GL_DRAW_FRAMEBUFFER_BINDING, GL_READ_FRAMEBUFFER_BINDING})
        {
            values.push_back(state(name));
        }
        const GLint active = state(GL_ACTIVE_TEXTURE);
        glActiveTexture(GL_TEXTURE0);
        values.push_back(state(GL_TEXTURE_BINDING_2D));
        glActiveTexture(active);
        return values;
    }

    void expectPixel(int x, const std::array<int, 4> &expected, const char *label)
    {
        std::array<unsigned char, 4> actual{};
        glReadPixels(x, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
        for (std::size_t channel = 0; channel < actual.size(); ++channel)
        {
            require(std::abs(static_cast<int>(actual[channel]) - expected[channel]) <= 2,
                std::string(label) + ": channel " + std::to_string(channel) + " actual "
                + std::to_string(actual[channel]) + " expected " + std::to_string(expected[channel]));
        }
    }

    float depth()
    {
        float value = 0;
        glReadPixels(32, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &value);
        return value;
    }
}

int main()
{
    try
    {
        Window window(64, 64, "Scene Render Test", false);
        ColorDepthTarget target;
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Incomplete framebuffer");
        glDisable(GL_DITHER);
        glDisable(GL_FRAMEBUFFER_SRGB);
        Shader shader("examples/scene_objects/shaders/scene.vert", "examples/scene_objects/shaders/scene.frag");
        Shader callerShader("examples/alpha_blending/shaders/color.vert", "examples/alpha_blending/shaders/color.frag");
        Mesh quad = createQuad();
        Mesh cube(CubeExample::vertices());
        ImageData image;
        image.width = 1;
        image.height = 1;
        image.pixels = {255, 255, 255, 255};
        Texture white(image);
        Texture callerTexture(image);
        Material blue(shader, glm::vec4(0, 0, 1, 1), &white);
        Material red(shader, glm::vec4(1, 0, 0, 0.5f), &white);
        Material green(shader, glm::vec4(0, 1, 0, 0.5f), &white);
        require(red.renderMode() == RenderMode::Opaque && red.cullMode() == CullMode::None,
            "Material defaults changed");
        red.setRenderMode(RenderMode::AlphaBlend);
        green.setRenderMode(RenderMode::AlphaBlend);
        Scene scene;
        auto &near = scene.createObject("near red");
        near.setRenderable(quad, red);
        near.transform.position.z = 0.2f;
        auto &back = scene.createObject("opaque blue");
        back.setRenderable(quad, blue);
        back.transform.position.z = -0.8f;
        auto &far = scene.createObject("far green");
        far.setRenderable(quad, green);
        far.transform.position.z = -0.2f;
        scene.createObject("empty object");
        Camera camera;
        Renderer renderer;

        // 故意设置非默认的调用者状态，确保Scene临时覆盖后恢复真实值，而非默认值。
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_GREATER);
        glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
        glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ONE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glFrontFace(GL_CW);
        callerShader.use();
        callerTexture.bind(0);
        callerTexture.bind(3);
        GLuint callerVao = 0;
        glGenVertexArrays(1, &callerVao);
        glBindVertexArray(callerVao);
        const auto callerState = snapshot();
        const auto render = [&]()
        {
            renderer.clear(0, 0, 0, 1);
            scene.render(renderer, camera, 1.0f);
            require(snapshot() == callerState, "Scene leaked render state or resource binding");
            require(glGetError() == GL_NO_ERROR, "Scene produced OpenGL error");
        };

        near.setActive(false);
        far.setActive(false);
        render();
        expectPixel(32, {0, 0, 255, 255}, "Disabled objects skipped");
        const float backgroundDepth = depth();
        near.setActive(true);
        far.setActive(true);
        render();
        expectPixel(32, {128, 64, 64, 255}, "Opaque first and sorted transparent colors");
        require(std::abs(depth() - backgroundDepth) < 0.00001f, "Transparent pass wrote depth");
        // 在同一场景每帧修改深度，证明没有缓存过期的排序结果。
        near.transform.position.z = -0.4f;
        render();
        expectPixel(32, {64, 128, 64, 255}, "Depth change updates transparent order");
        near.transform.position.z = 0.2f;
        back.transform.position.z = 0.6f;
        render();
        expectPixel(32, {0, 0, 255, 255}, "Opaque foreground occludes transparency");

        // 清空后改变创建顺序，复用同一批资源仍然得到相同结果。
        scene.clear();
        auto &farAgain = scene.createObject("far");
        farAgain.setRenderable(quad, green);
        farAgain.transform.position.z = -0.2f;
        auto &nearAgain = scene.createObject("near");
        nearAgain.setRenderable(quad, red);
        nearAgain.transform.position.z = 0.2f;
        auto &backAgain = scene.createObject("back");
        backAgain.setRenderable(quad, blue);
        backAgain.transform.position.z = -0.8f;
        render();
        expectPixel(32, {128, 64, 64, 255}, "Reordered scene reused live resources");
        // 空场景既不绘制也不清屏，更不能改变调用者状态。
        scene.clear();
        scene.render(renderer, camera, 1.0f);
        expectPixel(32, {128, 64, 64, 255}, "Empty scene does not clear image");
        require(snapshot() == callerState, "Empty scene changed state");

        Material solidGreen(shader, glm::vec4(0, 1, 0, 1), &white);
        Material solidRed(shader, glm::vec4(1, 0, 0, 1), &white);
        auto &left = scene.createObject("left mirrored plane");
        left.setRenderable(quad, solidGreen);
        left.transform.position.x = -0.65f;
        left.transform.scale = glm::vec3(-0.5f, 0.5f, 0.5f);
        auto &right = scene.createObject("right plane");
        right.setRenderable(quad, solidRed);
        right.transform.position.x = 0.65f;
        right.transform.scale = glm::vec3(0.5f);
        render();
        expectPixel(16, {0, 255, 0, 255}, "Shared mesh independent left transform");
        expectPixel(48, {255, 0, 0, 255}, "Shared mesh independent right transform");
        // 双面镜像平面与背面剔除立方体混用，剔除配置不得串到下一个物体。
        solidRed.setCullMode(CullMode::Back);
        right.setRenderable(cube, solidRed);
        right.transform.scale = glm::vec3(0.6f);
        render();
        expectPixel(16, {0, 255, 0, 255}, "Double-sided plane with culled cube");
        expectPixel(48, {255, 0, 0, 255}, "Outward cube remains visible");
        solidGreen.setCullMode(CullMode::Back);
        render();
        expectPixel(16, {0, 0, 0, 255}, "Mirrored plane is back facing");
        solidGreen.setCullMode(CullMode::None);
        right.setActive(false);
        render();
        expectPixel(16, {0, 255, 0, 255}, "Cull disabled again");
        expectPixel(48, {0, 0, 0, 255}, "Disabled cube not rendered");
        left.clearRenderable();
        render();
        expectPixel(16, {0, 0, 0, 255}, "Unbound object not rendered");
        const ObjectId rightId = right.id();
        require(scene.removeObject(rightId), "Delete failed");
        auto &replacement = scene.createObject("replacement reuses cube");
        replacement.setRenderable(cube, solidRed);
        replacement.transform.scale = glm::vec3(0.6f);
        render();
        expectPixel(32, {255, 0, 0, 255}, "Deletion preserved shared GPU resources");

        bool rejected = false;
        try { renderer.drawItems({RenderItem{}}, camera, 1.0f); }
        catch (const std::invalid_argument &) { rejected = true; }
        require(rejected && snapshot() == callerState, "Invalid item modified caller state");
        // 先解除借用，再让后面创建的测试材质析构；Scene清空不销毁任何GPU资源。
        scene.clear();
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &callerVao);
        require(glGetError() == GL_NO_ERROR, "Final OpenGL error");
        std::cout << "Scene rendering passed: pixels, sorting, shared resources, culling, deletion and state restoration" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Scene rendering failed: " << exception.what() << std::endl;
        return 1;
    }
}
