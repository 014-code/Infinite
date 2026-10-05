#include "../common/ExampleRun.h"
#include "../common/FreeCameraController.h"

#include "resources/ResourceManager.h"
#include "scene/ModelInstantiator.h"

#include <glm/gtc/quaternion.hpp>
#include <stdexcept>

int main(int argc, char *argv[])
{
    FreeCameraController cameraController;
    return runExample(argc, argv, "gltf_model",
        "Infinite - glTF Model (WASD / RMB look / R reset / Esc exit)",
        {0.025f, 0.035f, 0.06f, 1.0f},
        [&cameraController](Application &application, const std::filesystem::path &directory)
    {
        // glTF文件只描述模型数据和材质参数，预览Shader仍由应用明确选择。
        const auto model = application.resources().loadModel(
            directory / "assets/showcase.gltf",
            directory / "shaders/preview.vert",
            directory / "shaders/preview.frag");
        auto first = ModelInstantiator::instantiate(application.scene(), *model, "左侧模型");
        auto second = ModelInstantiator::instantiate(application.scene(), *model, "右侧模型");
        // 左侧物体覆盖窗口中点，通用冒烟测试可以直接确认真实模型像素。
        application.scene().findObject(first.rootId)->transform.position.x = 0.0f;
        application.scene().findObject(second.rootId)->transform.position.x = 1.05f;
        application.scene().findObject(second.rootId)->transform.scale = {-0.85f, 0.85f, 0.85f};

        // 模型资源共享，两个根节点只保存各自实例的Transform。
        application.scene().findObject(first.rootId)->setUpdateCallback(
            [](GameObject &object, float deltaTime)
            {
                object.transform.rotate(glm::angleAxis(deltaTime * 0.35f, glm::vec3(0, 1, 0)));
            });
        // 右侧保持静止并镜像缩放，用来对照实例独立性及背面剔除修正。
        application.camera().setPerspective(55.0f, 0.1f, 100.0f);
        cameraController.apply(application.camera());
        LOG_INFO("Loaded two independent glTF instances with shared GPU resources");
    }, ExampleFrameContent::DrawnScene,
    [&cameraController](Application &application, float deltaTime)
    {
        cameraController.update(application.camera(), application.input(), deltaTime,
            application.window().isFocused());
    });
}
