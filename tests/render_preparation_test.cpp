#include "TestSupport.h"
#include "support/UniformQueryProbe.h"
#include "support/ColorDepthTarget.h"
#include "graphics/camera/Camera.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Material.h"
#include "math/Transform.h"
#include "platform/Window.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <iostream>

int main()
{
    try
    {
        Window window(64, 64, "Render preparation benchmark", false);
        ColorDepthTarget target;
        Shader shader("examples/diffuse_lighting/shaders/diffuse.vert", "examples/diffuse_lighting/shaders/diffuse.frag");
        Mesh mesh(std::vector<Vertex>{{{-.5f, -.5f, 0}}, {{.5f, -.5f, 0}}, {{0, .5f, 0}}});
        Material opaque(shader), transparent(shader, glm::vec4(1, 1, 1, .5f));
        transparent.setRenderMode(RenderMode::AlphaBlend);
        opaque.setCorrectMirroredWinding(true);
        transparent.setCorrectMirroredWinding(true);
        // 128个物体复用一个Mesh/Shader，挂到16层父链上，放大重复矩阵计算的成本。
        std::array<Transform, 16> parents;
        for (std::size_t i = 1; i < parents.size(); ++i) { parents[i].setParent(&parents[i - 1]); }
        std::array<Transform, 128> transforms;
        std::vector<RenderItem> items;
        for (std::size_t i = 0; i < transforms.size(); ++i)
        {
            transforms[i].setParent(&parents.back());
            transforms[i].position.z = -static_cast<float>((i * 17) % 128) * .01f;
            items.push_back({&mesh, i % 2 ? &transparent : &opaque, &transforms[i]});
        }
        Camera camera;
        Renderer renderer;
        UniformQueryProbe probe;
        // 只测准备和提交路径，关闭光栅化以减少像素填充影响；像素正确性由其他回归测试负责。
        glEnable(GL_RASTERIZER_DISCARD);
        renderer.drawItems(items, camera, 1);
        std::cout << "Cold batch driver uniform queries: " << probe.calls() << '\n';
        std::array<double, 5> samples;
        probe.reset();
        for (auto &sample : samples)
        {
            glFinish();
            const auto start = std::chrono::steady_clock::now();
            for (int frame = 0; frame < 20; ++frame) { renderer.drawItems(items, camera, 1); }
            glFinish();
            sample = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 20;
        }
        std::sort(samples.begin(), samples.end());
        std::cout << "Warm queries (100 batches): " << probe.calls()
            << "; median ms/batch: " << samples[2] << '\n';
        require(probe.calls() == 0, "Warm draws repeated driver uniform lookup");
        glDisable(GL_RASTERIZER_DISCARD);
        require(glGetError() == GL_NO_ERROR, "Preparation benchmark produced GL error");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Render preparation test failed: " << error.what() << '\n';
        return 1;
    }
}
