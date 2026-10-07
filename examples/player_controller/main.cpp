#include "../common/ExampleRun.h"
#include "../textured_cube/CubeGeometry.h"

#include "FlatGroundMovement.h"
#include "PlayerInput.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"

#include <memory>
#include <cmath>

int main(int argc, char *argv[])
{
    return runExample(argc, argv, "player_controller",
        "Infinite - Flat Ground Demo (WASD / Space / Shift / R reset / Esc)",
        {0.025f, 0.035f, 0.055f, 1.0f},
        [](Application &application, const std::filesystem::path &directory)
        {
            auto shader = application.resources().loadShader(
                directory / "shaders/player.vert", directory / "shaders/player.frag");
            auto mesh = std::make_shared<Mesh>(CubeExample::vertices());
            auto playerMaterial = std::make_shared<Material>(shader, glm::vec4(0.15f, 0.65f, 1.0f, 1.0f));
            auto floorMaterial = std::make_shared<Material>(shader, glm::vec4(0.35f, 0.38f, 0.42f, 1.0f));
            playerMaterial->setCullMode(CullMode::Back);
            floorMaterial->setCullMode(CullMode::Back);

            auto &player = application.scene().createObject("player");
            // 本地-Z面使用不同颜色，便于观察示例自己的转向规则。
            auto playerVertices = CubeExample::vertices();
            for (std::size_t i = 6; i < 12; ++i) { playerVertices[i].color = {1.0f, 0.25f, 0.15f}; }
            player.setRenderable(std::make_shared<Mesh>(playerVertices), playerMaterial);
            player.transform.position = {0.0f, 0.5f, 0.0f};

            auto &floor = application.scene().createObject("floor");
            floor.setRenderable(mesh, floorMaterial);
            // 薄盒顶部为y=0，与玩家中心y=0.5对应，避免看起来悬空。
            floor.transform.position = {0.0f, -0.125f, 0.0f};
            floor.transform.scale = {20.0f, 0.25f, 20.0f};

            // 按键是本示例的选择：显式注册，而不是创建移动对象时自动修改输入配置。
            const PlayerExample::PlayerBindings bindings;
            PlayerExample::registerDefaultActions(application.actions(), bindings);
            PlayerExample::FlatGroundSettings settings;
            // 固定高度只代表“无限平地”约束，与上面的地板网格没有碰撞关系。
            // 走出可见地板仍会站在此高度；这不是通用角色物理系统。
            settings.groundHeight = 0.5f;
            auto movement = std::make_shared<PlayerExample::FlatGroundMovement>(settings);
            application.camera().setPerspective(55.0f, 0.1f, 100.0f);
            application.camera().setView({4.5f, 3.5f, 6.5f}, {0.0f, 0.5f, 0.0f});
            auto &light = application.directionalLight();
            light.direction = {-0.5f, -0.8f, -1.0f};
            light.intensity = 0.8f;
            light.ambient = {0.18f, 0.18f, 0.18f};
            // 移动状态必须跨帧存在，用shared_ptr随角色回调持有；不保存输入对象的借用指针。
            player.script().setUpdateCallback([movement, bindings, &application](GameObject &object, float deltaTime)
            {
                if (application.window().isFocused() && application.input().wasKeyPressed(Key::R))
                {
                    object.transform.position = {0, 0.5f, 0};
                    object.transform.setEulerAngles({0, 0, 0});
                    movement->reset(object.transform);
                    return;
                }
                const auto command = PlayerExample::sampleCommand(application.actions(),
                    application.input(), bindings, application.window().isFocused());
                movement->update(object.transform, command, deltaTime);
                // “朝向移动方向”只是此示例的表现策略，不属于移动规则。
                // 成功移动后再转向，零时长帧保持原朝向；模型本地前方约定为-Z。
                if (deltaTime > 0 && command.movement != glm::vec2(0))
                {
                    const float yaw = std::atan2(-command.movement.x, -command.movement.y);
                    object.transform.setRotation(glm::angleAxis(yaw, glm::vec3(0, 1, 0)));
                }
            });
        }, ExampleFrameContent::DrawnScene);
}
