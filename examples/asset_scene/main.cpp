#include "../common/ExampleRun.h"
#include "../common/FreeCameraController.h"

#include "scene/SceneSerializer.h"

int main(int argc, char *argv[])
{
    FreeCameraController cameraController;
    return runExample(argc, argv, "asset_scene",
        "Infinite - Asset Scene (WASD / RMB look / R reset / Esc exit)",
        {0.035f, 0.045f, 0.07f, 1.0f},
        [&cameraController](Application &application, const std::filesystem::path &directory)
    {
        // 场景引用OBJ和材质，材质再引用Shader与图片。路径逐层相对所属文件解析，
        // 因此双击exe或从其他工作目录启动，都能使用部署在exe旁的同一套资源。
        auto &scene = application.scene();
        const auto ids = SceneSerializer::load(scene,
            directory / "scenes/showcase.scene", application.resources());
        if (ids.size() != 2)
        {
            throw std::runtime_error("Asset showcase requires two objects");
        }

        auto &parent = *scene.findObject(ids[0]);
        auto &child = *scene.findObject(ids[1]);
        // 两个实例各自保留Transform，只共享几何与表面配置；也作为冒烟时的缓存检查。
        if (parent.mesh() != child.mesh() || parent.material() != child.material() ||
            child.transform.parent() != &parent.transform)
        {
            throw std::runtime_error("Asset showcase did not restore shared resources or hierarchy");
        }

        // 文件只保存数据，不序列化C++函数。加载返回的ID用来重新绑定应用行为。
        parent.script().setUpdateCallback([](GameObject &object, float deltaTime)
        {
            // angleAxis用“角度（弧度）+单位轴”构造增量四元数，rotate在局部空间累积。
            // 父节点转动时，子节点的位置也跟随旋转，形成绕大立方体运动的效果。
            object.transform.rotate(glm::angleAxis(deltaTime * 0.55f, glm::vec3(0, 1, 0)));
        });
        child.script().setUpdateCallback([](GameObject &object, float deltaTime)
        {
            // 子节点还可独立自转；世界矩阵会组合父节点和它自己的局部变换。
            object.transform.rotate(glm::angleAxis(deltaTime, glm::vec3(1, 0, 0)));
        });
        application.camera().setPerspective(55.0f, 0.1f, 100.0f);
        cameraController.apply(application.camera());
        LOG_INFO("Loaded scene: 2 objects share 1 OBJ, 1 Material, 1 Shader and 1 Texture");
    }, ExampleFrameContent::DrawnScene,
    [&cameraController](Application &application, float deltaTime)
    {
        // 摄像机按键方案复用示例公共代码，框架层不引入这个演示的交互规则。
        cameraController.update(application.camera(), application.input(), deltaTime,
            application.window().isFocused());
    });
}
