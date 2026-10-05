#include "common/ExampleRun.h"
#include "common/FreeCameraController.h"
#include "animation/AnimationPlayer.h"
#include "graphics/resources/Material.h"
#include <array>

int main(int argc, char *argv[])
{
    try
    {
        ExampleRun example(argc, argv);
        ApplicationConfig config;
        config.title = "Infinite - Skinned Rigged Figure | P: pause | WASD / RMB";
        config.visible = example.visible(); config.linearHdr = true;
        Application app(config);
        std::array<std::unique_ptr<AnimationPlayer>,2> players;
        bool paused = false;
        FreeCameraSettings settings;
        settings.startPosition = {0,1.5f,4}; settings.startYaw = glm::radians(-90.0f);
        settings.startPitch = glm::radians(-12.0f);
        FreeCameraController camera(settings);
        const auto directory = executableDirectory(argv[0]);
        auto callbacks = example.callbacks();
        callbacks.initialize = [&](Application &application)
        {
            auto &scene = application.scene();
            auto model = application.resources().loadPbrModel(directory / "assets/RiggedFigure.glb");
            for (std::size_t index = 0; index < players.size(); ++index)
            {
                const auto instance = ModelInstantiator::instantiate(scene,*model,"figure " + std::to_string(index));
                scene.findObject(instance.rootId)->transform.position.x = index == 0 ? -.8f : .8f;
                players[index] = std::make_unique<AnimationPlayer>(scene,model,instance);
                players[index]->play(scene,0);
            }
            // 共享同一份网格/材质/动画，右侧使用不同时间和速度，验证实例姿态互不干扰。
            players[1]->setSpeed(.55f);
            players[1]->seek(scene,model->animations()[0].duration() * .5);
            PlaneOptions floor; floor.size = {8,8};
            floor.material = application.pbrResources().createMaterial({.08f,.1f,.13f,1});
            scene.createPlane(floor).transform.position.y = -.02f;
            scene.lighting().mainLight().direction = {-.4f,-.7f,-1};
            scene.lighting().mainLight().intensity = 3;
            camera.apply(application.camera());
        };
        callbacks.update = [&](Application &application,float dt)
        {
            if (application.input().wasKeyPressed(Key::P))
            {
                paused = !paused;
                for (auto &player : players) { if (paused) { player->pause(); } else { player->resume(); } }
            }
            for (auto &player : players) { player->update(application.scene(),dt); }
            camera.update(application.camera(),application.input(),dt,application.window().isFocused());
        };
        app.run(callbacks);
        return 0;
    }
    catch (const std::exception &error) { LOG_ERROR(error.what()); return 1; }
}
