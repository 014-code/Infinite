#include "ShowcaseScene.h"

#include "graphics/resources/Material.h"

#include <glm/gtc/constants.hpp>
#include <memory>
#include <string>

namespace MaterialShowcase
{
    namespace
    {
        struct ModelPlacement
        {
            const char *name;
            const char *directory;
            glm::vec3 position;
            glm::vec3 scale;
            glm::vec3 rotation;
        };

        // 不同模型的原始单位和朝向可能不同，展示层只调整构图，不修改Model资源本身。
        // Avocado原始单位很小、Lantern原始单位很大，因此这里做展示归一化。
        constexpr std::array<ModelPlacement, 3> kPlacements{{
            {"Avocado", "Avocado", {-2.8f, 0.55f, 0.5f}, glm::vec3(12.0f), {0.0f, 0.25f, 0.0f}},
            {"Barramundi Fish", "BarramundiFish", {0.0f, 0.75f, 0.0f}, glm::vec3(3.8f), {0.0f, -0.35f, 0.0f}},
            {"Lantern", "Lantern", {2.8f, 0.55f, -0.1f}, glm::vec3(0.10f), {0.0f, 0.5f, 0.0f}}
        }};

        GameObject *findRoot(Scene &scene, const ModelInstance &instance)
        {
            auto *root = scene.findObject(instance.rootId);
            if (root == nullptr) { throw std::logic_error("Material showcase model root disappeared"); }
            return root;
        }
    }

    FreeCameraSettings cameraSettings()
    {
        FreeCameraSettings result;
        result.moveSpeed = 4.0f;
        // 开场镜头把鱼放在画面中心附近，冒烟检查可以稳定读到模型像素。
        result.startPosition = {5.8f, 3.4f, 10.5f};
        result.startYaw = glm::radians(-119.0f);
        result.startPitch = glm::radians(-14.0f);
        return result;
    }

    Exhibits createScene(Application &application, const std::filesystem::path &directory)
    {
        const auto vertexShader = directory / "shaders/material.vert";
        const auto fragmentShader = directory / "shaders/material.frag";
        Exhibits result;
        for (std::size_t i = 0; i < kPlacements.size(); ++i)
        {
            const auto &placement = kPlacements[i];
            // 这些GLB由固定版本脚本预置到示例资源目录，运行时不访问网络。
            const auto model = application.resources().loadModel(
                directory / "assets/models" / placement.directory / "preview.glb",
                vertexShader, fragmentShader);
            result.instances[i] = ModelInstantiator::instantiate(
                application.scene(), *model, std::string("model/") + placement.name);
            auto *root = findRoot(application.scene(), result.instances[i]);
            root->transform.position = placement.position;
            root->transform.scale = placement.scale;
            root->transform.setEulerAngles(placement.rotation);
        }

        // 平台的布局属于示例，水平几何复用框架：Plane位于XZ平面，正面朝+Y。
        // size是完整宽/深，不是半尺寸；保留展示Shader的sRGB输出约定。
        auto shader = application.resources().loadShader(vertexShader, fragmentShader);
        auto material = std::make_shared<Material>(shader, glm::vec4(0.06f, 0.075f, 0.09f, 1.0f));
        material->setCullMode(CullMode::Back);
        material->setShaderOutputsSrgb(true);
        PlaneOptions platformOptions;
        platformOptions.name = "showcase platform";
        platformOptions.size = {8.4f, 4.4f};
        platformOptions.material = material;
        auto &platform = application.scene().createPlane(platformOptions);
        platform.transform.position = {0.0f, -1.15f, 0.0f};
        result.platformId = platform.id();

        auto &light = application.directionalLight();
        light.direction = {-0.6f, -1.0f, -0.8f};
        light.color = {1.0f, 0.96f, 0.89f};
        light.intensity = 0.85f;
        light.ambient = {0.18f, 0.20f, 0.23f};
        application.camera().setPerspective(42.0f, 0.1f, 150.0f);
        FreeCameraController(cameraSettings()).apply(application.camera());
        return result;
    }

    void updateExhibits(Application &application, const Exhibits &exhibits, float deltaTime)
    {
        if (!exhibits.rotating) { return; }
        for (const auto &instance : exhibits.instances)
        {
            if (auto *root = application.scene().findObject(instance.rootId))
            {
                root->transform.rotateEuler({0.0f, deltaTime * 0.22f, 0.0f});
            }
        }
    }
}
