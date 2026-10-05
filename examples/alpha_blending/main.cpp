#include "../common/ExampleRun.h"

#include "graphics/resources/ImageLoader.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"
#include "math/Transform.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
    Mesh createQuad()
    {
        // 两个三角形组成一个朝向摄像机的平面，所有顶点使用白色以保留材质颜色。
        const std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };
        return Mesh(vertices, {0, 1, 2, 2, 3, 0});
    }

    ImageData createSoftCircleImage()
    {
        // 示例直接生成一张白色圆形RGBA图片，避免把临时演示素材放进框架层。
        // 圆外Alpha为0，边缘做一圈平滑过渡，便于观察透明混合而非矩形贴图边界。
        constexpr int imageSize = 128;
        ImageData image;
        image.width = imageSize;
        image.height = imageSize;
        image.pixels.resize(static_cast<size_t>(imageSize) * imageSize * ImageData::channels);

        for (int y = 0; y < imageSize; ++y)
        {
            for (int x = 0; x < imageSize; ++x)
            {
                const float dx = (static_cast<float>(x) + 0.5f - imageSize * 0.5f) / (imageSize * 0.5f);
                const float dy = (static_cast<float>(y) + 0.5f - imageSize * 0.5f) / (imageSize * 0.5f);
                const float radius = std::sqrt(dx * dx + dy * dy);
                const float alpha = 1.0f - std::clamp((radius - 0.82f) / 0.18f, 0.0f, 1.0f);
                const size_t offset = (static_cast<size_t>(y) * imageSize + x) * ImageData::channels;
                image.pixels[offset] = 255;
                image.pixels[offset + 1] = 255;
                image.pixels[offset + 2] = 255;
                image.pixels[offset + 3] = static_cast<unsigned char>(alpha * 255.0f);
            }
        }
        return image;
    }
}

int main(int argc, char *argv[])
{
    return runExample(argc, argv, "alpha_blending", "Infinite - Alpha Blending Example",
        {0.035f, 0.045f, 0.07f, 1.0f}, [](Application &application, const std::filesystem::path &directory)
    {
        // 创建窗口和OpenGL上下文后加载资源。透明演示不使用背面剔除，避免平面朝向造成整片消失。
        auto colorShader = application.resources().loadShader(
            (directory / "shaders/color.vert"),
            (directory / "shaders/color.frag"));
        auto textureShader = application.resources().loadShader(
            (directory / "shaders/texture.vert"),
            (directory / "shaders/texture.frag"));

        auto quad = std::make_shared<Mesh>(createQuad());
        // 不透明底板和透明圆形共用同一个几何体，外观由各自Material决定。
        auto backgroundMaterial = std::make_shared<Material>(colorShader, glm::vec4(0.16f, 0.2f, 0.28f, 1.0f));
        auto wallMaterial = std::make_shared<Material>(colorShader, glm::vec4(0.85f, 0.85f, 0.78f, 1.0f));
        // 圆形图片由示例在内存中生成，不能按文件路径缓存，因此仍直接创建Texture。
        auto circleTexture = std::make_shared<Texture>(createSoftCircleImage());
        auto warmMaterial = std::make_shared<Material>(textureShader, glm::vec4(1.0f, 0.22f, 0.12f, 0.72f), circleTexture);
        auto coolMaterial = std::make_shared<Material>(textureShader, glm::vec4(0.12f, 0.48f, 1.0f, 0.72f), circleTexture);

        Transform backgroundTransform;
        backgroundTransform.scale = glm::vec3(3.4f, 2.3f, 1.0f);
        backgroundTransform.position.z = -0.55f;

        // 前方窄条是一个不透明遮挡物。即使透明圆形后绘制，也不能覆盖这条遮挡物。
        Transform wallTransform;
        wallTransform.scale = glm::vec3(0.16f, 1.6f, 1.0f);
        wallTransform.position = glm::vec3(0.55f, 0.0f, 0.4f);

        Transform warmTransform;
        warmTransform.scale = glm::vec3(1.35f);
        warmTransform.position = glm::vec3(-0.32f, 0.0f, -0.05f);
        Transform coolTransform;
        coolTransform.scale = glm::vec3(1.35f);
        coolTransform.position = glm::vec3(0.32f, 0.0f, 0.18f);

        // 场景共享持有网格和材质，材质继续持有Shader/Texture，初始化返回后仍有效。
        warmMaterial->setRenderMode(RenderMode::AlphaBlend);
        coolMaterial->setRenderMode(RenderMode::AlphaBlend);
        auto &scene = application.scene();
        const auto add = [&](const char *name, const std::shared_ptr<Material> &material, const Transform &transform)
        {
            auto &object = scene.createObject(name);
            object.setRenderable(quad, material);
            object.transform = transform;
        };
        // 故意先提交近处，再提交远处，实际绘制前由Scene的排序流程调整。
        add("cool circle", coolMaterial, coolTransform);
        add("warm circle", warmMaterial, warmTransform);
        add("background", backgroundMaterial, backgroundTransform);
        add("wall", wallMaterial, wallTransform);
        // Scene先画不透明底板并写深度，再按当前摄像机视角从远到近绘制透明组。
        // 透明阶段关闭深度写入，结束后自动恢复状态，不泄漏到下一帧。
        // Application负责主循环与OpenGL检查，不把上述透明演示内容放进框架。
    });
}
