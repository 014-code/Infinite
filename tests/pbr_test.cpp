#include "TestSupport.h"
#include "support/ColorDepthTarget.h"
#include "graphics/rendering/HdrPipeline.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Material.h"
#include "graphics/resources/Texture.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/camera/Camera.h"
#include "graphics/lighting/SceneLighting.h"
#include "resources/PbrResources.h"
#include "platform/Window.h"
#include "math/Transform.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <glm/geometric.hpp>

namespace
{
    std::array<float, 4> pixel()
    {
        std::array<float, 4> result;
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_FLOAT, result.data());
        return result;
    }
    void near(float actual, float expected, const char *label)
    { require(std::isfinite(actual) && std::abs(actual - expected) < .015f, label); }
}

int main()
{
    try
    {
        PbrParameters invalid;
        invalid.roughness = -1;
        expectThrow<std::invalid_argument>([&] { invalid.validate(); }, "Negative roughness accepted");
        invalid.roughness = .5f; invalid.emission.x = std::numeric_limits<float>::quiet_NaN();
        expectThrow<std::invalid_argument>([&] { invalid.validate(); }, "NaN emission accepted");
        Window window(64, 64, "PBR pixels", false);
        ColorDepthTarget target;
        glDisable(GL_DITHER);
        HdrPipeline pipeline;
        // .25线性值应该编码为约.537；调用方开启sRGB不能让输出被编码两次。
        glEnable(GL_FRAMEBUFFER_SRGB);
        pipeline.render(64, 64, {.25f, .25f, .25f, 1}, [] {}, 1, false);
        near(pixel()[0], .5371f, "Linear/sRGB output mismatch");
        require(glIsEnabled(GL_FRAMEBUFFER_SRGB), "HDR leaked sRGB state");
        pipeline.render(32, 32, {3, 3, 3, 1}, [] {});
        near(pixel()[0], .8808f, "HDR resize/tone mapping mismatch");
        GLint fbo; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        expectThrow<std::runtime_error>([&]
        { pipeline.render(64, 64, {0, 0, 0, 1}, [] { throw std::runtime_error("draw failed"); }); }, "Callback failure swallowed");
        GLint after; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &after);
        require(after == fbo, "HDR failure leaked FBO");

        PbrResources resources;
        PbrParameters parameters;
        parameters.unlit = true;
        auto material = resources.createMaterial({1, 0, 0, .5f}, parameters);
        material->setCullMode(CullMode::None);
        material->setRenderMode(RenderMode::AlphaBlend);
        std::vector<Vertex> vertices{{{-1, -1, 0}, glm::vec3(1), {0, 0}},
            {{1, -1, 0}, glm::vec3(1), {1, 0}}, {{1, 1, 0}, glm::vec3(1), {1, 1}},
            {{-1, 1, 0}, glm::vec3(1), {0, 1}}};
        Mesh mesh(vertices, {0, 1, 2, 2, 3, 0});
        Transform transform;
        Renderer renderer;
        Camera camera;
        camera.setOrthographic(2, .1f, 10);
        const auto draw = [&] { renderer.drawItems({{&mesh, material.get(), &transform}}, camera, 1); };
        pipeline.render(64, 64, {0, 0, 1, 1}, [&]
        {
            draw();
            near(pixel()[0], .5f, "Blend must occur in linear red");
            near(pixel()[2], .5f, "Blend must occur in linear blue");
        }, 1, false);
        near(pixel()[0], .73536f, "Blend was encoded before compositing");
        material->setRenderMode(RenderMode::AlphaMask);
        parameters.alphaCutoff = .6f; material->setPbrParameters(parameters);
        pipeline.render(64, 64, {0, 0, 1, 1}, draw, 1, false);
        near(pixel()[0], 0, "MASK below cutoff did not discard");
        parameters.alphaCutoff = .5f; material->setPbrParameters(parameters);
        pipeline.render(64, 64, {0, 0, 1, 1}, draw, 1, false);
        near(pixel()[0], 1, "MASK at cutoff discarded");

        // 所有新增纹理单元和活动单元都必须恢复，而非只恢复0号。
        ImageData image; image.width = image.height = 1; image.pixels = {128, 128, 255, 255};
        auto normal = std::make_shared<Texture>(image);
        material->setPbrTexture(PbrTextureSlot::Normal, normal);
        for (GLuint unit = 0; unit < 5; ++unit) { normal->bind(unit); }
        std::array<GLint, 5> before;
        for (int i = 0; i < 5; ++i) { glActiveTexture(GL_TEXTURE0 + i); glGetIntegerv(GL_TEXTURE_BINDING_2D, &before[i]); }
        pipeline.render(64, 64, {0, 0, 0, 1}, draw);
        GLint active; glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        require(active == GL_TEXTURE4, "PBR active unit not restored");
        for (int i = 0; i < 5; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i); GLint binding; glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
            require(binding == before[i], "PBR texture binding not restored");
        }
        parameters.unlit = false; parameters.emission = {4, 0, 0}; material->setPbrParameters(parameters);
        pipeline.render(64, 64, {0, 0, 0, 1}, [&] { draw(); require(pixel()[0] > 3.9f, "Emission clipped before output"); });

        material->setRenderMode(RenderMode::Opaque);
        material->setPbrTexture(PbrTextureSlot::Normal, {});
        material->setBaseColor({.5f, .5f, .5f, 1});
        parameters.emission = glm::vec3(0);
        SceneLighting light;
        light.ambient() = glm::vec3(0);
        light.mainLight().direction = {0, 0, -1};
        light.mainLight().intensity = 1;
        float linearPixel = 0;
        const auto renderLit = [&](const Mesh &geometry)
        {
            pipeline.render(64, 64, {0, 0, 0, 1}, [&]
            {
                renderer.drawItems({{&geometry, material.get(), &transform}}, camera, 1, light);
                linearPixel = pixel()[0];
            });
            return linearPixel;
        };
        // 中心像素不恰好位于世界原点：使用正交像素中心坐标建立独立CPU参考。
        const glm::vec3 n(0, 0, 1), l(0, 0, 1);
        const auto v = glm::normalize(camera.position() - glm::vec3(1.0f / 64, 1.0f / 64, 0));
        const auto h = glm::normalize(v + l);
        for (const float metal : {0.0f, 1.0f})
        {
            for (const float rough : {.4f, .8f})
            {
                parameters.metallic = metal; parameters.roughness = rough;
                material->setPbrParameters(parameters);
                const float a2 = std::pow(rough, 4), nh = glm::dot(n, h), nv = glm::dot(n, v);
                const float d = nh * nh * (a2 - 1) + 1;
                const float distribution = a2 / (3.14159265f * d * d);
                const float visibility = .5f / (std::sqrt(nv * nv * (1 - a2) + a2) + nv);
                const float f0 = metal == 0 ? .04f : .5f;
                const float fresnel = f0 + (1 - f0) * std::pow(1 - glm::dot(v, h), 5);
                const float expected = (1 - fresnel) * (1 - metal) * .5f / 3.14159265f + distribution * visibility * fresnel;
                near(renderLit(mesh), expected, "GGX/Smith/Fresnel reference mismatch");
            }
        }
        parameters.metallic = 0; parameters.roughness = .4f;
        material->setPbrParameters(parameters);
        const float frontal = renderLit(mesh);
        camera.setView({2, 0, 3}, {0, 0, 0});
        require(renderLit(mesh) < frontal * .8f, "Specular did not respond to camera movement");
        camera.setView({0, 0, 3}, {0, 0, 0});

        // AO黑图不应压暗直射光；关闭直射后才压暗环境近似项。
        image.pixels = {0, 0, 0, 255};
        auto black = std::make_shared<Texture>(image);
        material->setPbrTexture(PbrTextureSlot::Occlusion, black);
        near(renderLit(mesh), frontal, "AO incorrectly darkened direct light");
        light.ambient() = glm::vec3(1); light.mainLight().intensity = 0;
        near(renderLit(mesh), 0, "AO did not affect environment term");
        material->setPbrTexture(PbrTextureSlot::Occlusion, {});
        near(renderLit(mesh), .5f, "Missing AO map should use one");
        light.ambient() = glm::vec3(0); light.mainLight().intensity = 1;

        // 法线图向+X偏转：+Z灯贡献减少；导数TBN与显式切线的结果须一致。
        image.pixels = {218, 128, 218, 255};
        auto tilted = std::make_shared<Texture>(image);
        material->setPbrTexture(PbrTextureSlot::Normal, tilted);
        const auto tiltedPixel = renderLit(mesh);
        require(tiltedPixel < frontal * .7f, "Normal map did not change lighting");
        for (auto &vertex : vertices) { vertex.tangent = {1, 0, 0, 1}; }
        Mesh explicitTangents(vertices, {0, 1, 2, 2, 3, 0});
        near(renderLit(explicitTangents), tiltedPixel, "Explicit and derivative tangent frames disagree");
        parameters.normalScale = 0; material->setPbrParameters(parameters);
        near(renderLit(explicitTangents), frontal, "Normal scale zero is not flat");
        parameters.normalScale = 1; material->setPbrParameters(parameters);
        light.mainLight().direction = {-1, 0, -.2f};
        const auto rightLit = renderLit(explicitTangents);
        transform.scale.x = -1;
        require(renderLit(explicitTangents) < rightLit * .2f, "Mirrored tangent direction was not transformed");
        transform.scale.x = 1;
        for (auto &vertex : vertices) { vertex.tangent = glm::vec4(0); vertex.uv = glm::vec2(0); }
        Mesh degenerateUV(vertices, {0, 1, 2, 2, 3, 0});
        require(std::isfinite(renderLit(degenerateUV)), "Degenerate UV generated non-finite lighting");
        require(glGetError() == GL_NO_ERROR, "PBR/HDR generated GL error");
        std::cout << "PBR passed: HDR, encoding, blending, masking, emission and state restoration\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
