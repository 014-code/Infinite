#include "TestSupport.h"
#include "support/ColorDepthTarget.h"
#include "assets/GltfLoader.h"
#include "resources/ResourceManager.h"
#include "scene/ModelInstantiator.h"
#include "scene/Scene.h"
#include "graphics/resources/Material.h"
#include "graphics/resources/Texture.h"
#include "graphics/rendering/HdrPipeline.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/camera/Camera.h"
#include "platform/Window.h"
#include <cmath>
#include <iostream>

int main()
{
    try
    {
        const std::filesystem::path directory = "tests/fixtures/gltf_advanced";
        ModelLoadOptions options; options.pbrMaterials = true;
        auto data = GltfLoader::load(directory / "pbr.glb", options);
        const auto &surface = data.materials.at(0);
        require(std::abs(surface.pbr.metallic - .7f) < .0001f && std::abs(surface.pbr.roughness - .6f) < .0001f,
            "glTF PBR factors not imported");
        require(surface.pbrTextures == std::array<int, 4>{0, 0, 0, 0} && surface.doubleSided && surface.pbr.emission.y == .2f,
            "glTF PBR texture slots/emission not imported");
        const auto &vertex = data.primitives.at(0).mesh.vertices.at(0);
        require(vertex.color.x == .5f && vertex.color.y == .25f && vertex.colorAlpha == .5f && vertex.tangent.w == 1,
            "glTF RGBA/TANGENT not imported");
        for (const auto *name : {"bad-normal-uv.glb", "bad-roughness.glb"})
        { expectThrow<std::runtime_error>([&] { GltfLoader::load(directory / name, options); }, "Invalid PBR data accepted"); }
        require(GltfLoader::load(directory / "mask.glb", options).materials[0].alphaMode == ModelAlphaMode::Mask,
            "MASK descriptor lost");
        require(GltfLoader::load(directory / "blend.glb", options).materials[0].alphaMode == ModelAlphaMode::Blend,
            "BLEND descriptor lost");
        expectThrow<std::runtime_error>([&] { GltfLoader::load(directory / "mask.glb"); }, "Preview mode silently accepted MASK");

        Window window(64, 64, "glTF PBR test", false);
        ColorDepthTarget target;
        ResourceManager resources;
        auto model = resources.loadPbrModel(directory / "pbr.glb");
        require(model == resources.loadPbrModel(directory / "../gltf_advanced/pbr.glb"), "PBR cache key not normalized");
        const auto &material = *model->primitives().at(0).material;
        require(material.pbrParameters() && !material.shaderOutputsSrgb() && material.cullMode() == CullMode::None,
            "Runtime PBR material mode is wrong");
        require(material.texture()->isSrgb() && !material.pbrTexture(PbrTextureSlot::Normal)->isSrgb(),
            "Same image did not get separate color/data formats");
        require(material.texture() == material.pbrTexture(PbrTextureSlot::Emission) &&
            material.pbrTexture(PbrTextureSlot::Normal) == material.pbrTexture(PbrTextureSlot::Occlusion),
            "Matching image/sampler/color-space textures not shared");
        require(material.texture() != material.pbrTexture(PbrTextureSlot::Normal), "Color/data textures aliased");
        auto mask = resources.loadPbrModel(directory / "mask.glb");
        auto blend = resources.loadPbrModel(directory / "blend.glb");
        require(mask->primitives()[0].material->renderMode() == RenderMode::AlphaMask &&
            blend->primitives()[0].material->renderMode() == RenderMode::AlphaBlend, "Runtime alpha modes lost");

        Scene scene;
        auto unlit = resources.loadPbrModel(directory / "unlit.glb");
        auto instance = ModelInstantiator::instantiate(scene, *unlit);
        auto second = ModelInstantiator::instantiate(scene, *unlit);
        scene.findObject(second.rootId)->transform.position.x = 10;
        require(scene.findObject(instance.nodeIds[1])->mesh() == nullptr, "Model node topology unexpectedly changed");
        HdrPipeline pipeline;
        Camera camera; camera.setOrthographic(2, .1f, 100);
        Renderer renderer;
        glDisable(GL_DITHER);
        pipeline.render(64, 64, {0, 0, 0, 1}, [&] { scene.render(renderer, camera, 1); }, 1, false);
        float color[4]; glReadPixels(32, 32, 1, 1, GL_RGBA, GL_FLOAT, color);
        // sRGB128 -> 线性.21586，再乘baseColor .5和vertexColor .5/.25/1，最后只编码一次。
        const auto encode = [](float value) { return 1.055f * std::pow(value, 1.0f / 2.4f) - .055f; };
        require(std::abs(color[0] - encode(.21586f * .25f)) < .015f &&
            std::abs(color[1] - encode(.21586f * .125f)) < .015f && std::abs(color[3] - 1) < .01f,
            "Unlit/RGBA/sRGB reference pixel mismatch");
        // 关闭和改变灯光都不能影响KHR_materials_unlit；发光也不能被错误叠加。
        scene.lighting().ambient() = glm::vec3(100);
        pipeline.render(64, 64, {0, 0, 0, 1}, [&] { scene.render(renderer, camera, 1); }, 1, false);
        float after[4]; glReadPixels(32, 32, 1, 1, GL_RGBA, GL_FLOAT, after);
        require(std::abs(after[0] - color[0]) < .001f, "Unlit was affected by scene lights");
        resources.clear();
        require(resources.modelCount() == 0 && unlit->primitives()[0].material->texture(), "Cache clear invalidated live model");
        require(glGetError() == GL_NO_ERROR, "PBR model produced GL error");
        std::cout << "glTF PBR passed: core factors, formats, alpha, unlit, vertex data and resource sharing\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
