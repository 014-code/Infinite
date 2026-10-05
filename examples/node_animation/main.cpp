#include "common/ExampleRun.h"
#include "common/FreeCameraController.h"
#include "animation/AnimationStateMachine.h"
#include "graphics/resources/Material.h"
#include <array>

int main(int argc, char *argv[])
{
    try
    {
        ExampleRun example(argc, argv);
        ApplicationConfig config;
        config.title = "Infinite - Node animation | P: pause | Enter: restart | WASD / RMB";
        config.visible = example.visible(); config.linearHdr = true;
        Application app(config);
        // 播放器在Application之后创建、之前销毁，不让它持有的GPU资源越过窗口生命周期。
        std::array<std::unique_ptr<AnimationStateMachine>, 2> players;
        bool paused = false;
        FreeCameraSettings settings;
        settings.startPosition = {0, 3, 9}; settings.startYaw = glm::radians(-90.0f);
        settings.startPitch = glm::radians(-10.0f);
        FreeCameraController camera(settings);
        const auto directory = executableDirectory(argv[0]);
        auto callbacks = example.callbacks();
        callbacks.initialize = [&](Application &application)
        {
            auto &scene = application.scene();
            const auto model = application.resources().loadPbrModel(directory / "assets/kinetic.glb");
            for (std::size_t index = 0; index < players.size(); ++index)
            {
                const auto instance = ModelInstantiator::instantiate(scene, *model, "kinetic " + std::to_string(index));
                scene.findObject(instance.rootId)->transform.position.x = index == 0 ? -2.0f : 2.0f;
                players[index] = std::make_unique<AnimationStateMachine>(scene, model, instance);
                players[index]->addState("float_spin", 0);
                players[index]->addState("spin_only", 1);
                players[index]->setInitialState(scene, "float_spin");
            }
            players[1]->player().setSpeed(.6f);
            players[1]->player().seek(scene, 2);
            PlaneOptions floor; floor.size = {14, 10};
            floor.material = application.pbrResources().createMaterial({.08f, .1f, .13f, 1});
            scene.createPlane(floor).transform.position.y = -.05f;
            scene.lighting().mainLight().direction = {-.3f, -.7f, -1};
            scene.lighting().mainLight().intensity = 3;
            camera.apply(application.camera());
        };
        callbacks.update = [&](Application &application, float dt)
        {
            if (application.input().wasKeyPressed(Key::P))
            {
                paused = !paused;
                for (auto &player : players)
                { if (paused) { player->player().pause(); } else { player->player().resume(); } }
            }
            if (application.input().wasKeyPressed(Key::Enter))
            {
                for (auto &player : players)
                { player->transitionTo(application.scene(), "float_spin", .35f); }
                paused = false;
            }
            // 动画只改模型子节点的局部TRS；用户可独立移动实例根节点。
            for (auto &player : players) { player->update(application.scene(), dt); }
            camera.update(application.camera(), application.input(), dt, application.window().isFocused());
        };
        app.run(callbacks);
        return 0;
    }
    catch (const std::exception &error) { LOG_ERROR(error.what()); return 1; }
}
