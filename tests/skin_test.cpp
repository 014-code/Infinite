#include <GL/glew.h>
#include "TestSupport.h"
#include "support/ColorDepthTarget.h"
#include "animation/AnimationPlayer.h"
#include "assets/GltfLoader.h"
#include "resources/ResourceManager.h"
#include "scene/Scene.h"
#include "scene/SkinBinding.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Material.h"
#include "graphics/resources/Shader.h"
#include "graphics/camera/Camera.h"
#include "graphics/rendering/HdrPipeline.h"
#include "graphics/rendering/Renderer.h"
#include "platform/Window.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <iostream>

int main()
{
    try
    {
        const auto translate = [](float x, float y) { return glm::translate(glm::mat4(1), glm::vec3(x,y,0)); };
        const auto palette = buildSkinPalette(translate(10,2), {translate(10,3)}, {translate(0,-1)});
        const auto point = palette[0] * glm::vec4(1,2,3,1);
        require(glm::length(point - glm::vec4(1,2,3,1)) < .0001f, "Mesh/joint/bind spaces multiplied incorrectly");
        expectThrow<std::invalid_argument>([&] { buildSkinPalette(glm::mat4(0), {glm::mat4(1)}, {glm::mat4(1)}); }, "Singular mesh accepted");
        const std::filesystem::path directory = "tests/fixtures/skin";
        ModelLoadOptions options; options.pbrMaterials = true;
        auto data = GltfLoader::load(directory / "ribbon.glb", options);
        require(data.skins.size() == 1 && data.skins[0].joints.size() == 2 && data.nodes[3].skin == 0,
            "Skin/node mapping lost");
        require(data.primitives[0].skinVertices.size() == 10, "Skin attributes count mismatch");
        const auto generated = GltfLoader::load(directory / "generated-normal.glb", options);
        require(generated.primitives[0].skinVertices.size() == generated.primitives[0].mesh.vertices.size() &&
            generated.primitives[0].skinVertices.size() == 24, "Flat-normal expansion lost skin attributes");
        require(GltfLoader::load(directory / "identity-bind.glb", options).skins[0].inverseBind[1] == glm::mat4(1),
            "Missing inverse binds should default to identity");
        for (const auto *name : {"bad-joint", "bad-weight", "bad-matrix", "missing-weights", "duplicate-joint"})
        { expectThrow<std::runtime_error>([&] { GltfLoader::load(directory / (std::string(name)+".glb"), options); }, "Invalid skin accepted"); }
        expectThrow<std::runtime_error>([&] { GltfLoader::load(directory / "ribbon.glb"); }, "Preview silently accepted skin");

        Window window(64,64,"Skin test",false);
        ColorDepthTarget target;
        ResourceManager resources;
        auto model = resources.loadPbrModel(directory / "ribbon.glb");
        const auto &mesh = *model->primitives()[0].mesh;
        const auto &shader = model->primitives()[0].material->shader();
        require(mesh.hasSkinAttributes() && mesh.maximumJoint() == 1 && shader.matrixArrayCapacity("jointMatrices[0]") >= 2,
            "GPU skin layout/capacity mismatch");
        Scene scene;
        auto one = ModelInstantiator::instantiate(scene, *model);
        auto two = ModelInstantiator::instantiate(scene, *model);
        AnimationPlayer a(scene,model,one), b(scene,model,two);
        a.play(scene,0); b.play(scene,0);
        scene.findObject(two.rootId)->transform.position.x = 20;
        Camera camera; camera.setOrthographic(5,.1f,100);
        Renderer renderer; HdrPipeline hdr;
        std::array<unsigned char,64*64*4> before{}, after{};
        const auto render = [&] { hdr.render(64,64,{0,0,0,1},[&] { scene.render(renderer,camera,1); }); };
        render(); glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,before.data());
        a.seek(scene,1);
        render(); glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,after.data());
        int changed = 0;
        for (std::size_t index=0; index<before.size(); index+=4)
        { if (std::abs(int(before[index+2])-int(after[index+2])) > 10) { ++changed; } }
        require(changed > 30, "GPU skinning produced no visible deformation");
        require(scene.findObject(two.nodeIds[2])->transform.rotation().w == 1, "Skin animation leaked between instances");
        // CPU参考点：tip关节绕(0,1,0)转90度，原(0,2,0)应到(-1,1,0)。
        const auto *rendered = scene.findObject(one.objectIds.back());
        auto matrices = rendered->renderable().skin()->palette(scene, rendered->transform);
        const auto tip = matrices[1] * glm::vec4(0,2,0,1);
        require(glm::length(tip-glm::vec4(-1,1,0,1)) < .0001f, "Animated joint palette differs from reference");
        // 同一Shader接着画无Skin的物体时必须清除skinEnabled，不能继承上一帧关节矩阵。
        scene.findObject(one.rootId)->transform.position.x = 20;
        auto &staticObject = scene.createObject("static copy");
        staticObject.setRenderable(model->primitives()[0].mesh,model->primitives()[0].material);
        render(); glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,after.data());
        require(before == after, "Static draw retained previous skin uniforms");
        scene.removeObject(one.nodeIds[2]);
        expectThrow<std::runtime_error>([&] { render(); }, "Deleted joint was silently dereferenced/ignored");
        require(glGetError() == GL_NO_ERROR, "Skin upload/draw caused GL error");
        std::cout << "Skin tests passed: parsing, weights, spaces, GPU deformation, sharing and static-state reset\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
