#include "common/ExampleRun.h"
#include "common/FreeCameraController.h"
#include "graphics/resources/Material.h"

int main(int argc,char *argv[])
{
    try
    {
        ExampleRun example(argc,argv);
        ApplicationConfig config;
        config.title="Infinite - Directional shadows | H: toggle shadows | WASD / RMB";
        config.visible=example.visible(); config.linearHdr=true;
        config.directionalShadow=DirectionalShadowSettings{};
        Application app(config);
        FreeCameraSettings settings;
        settings.startPosition={5,4,7}; settings.startYaw=glm::radians(-125.0f); settings.startPitch=glm::radians(-22.0f);
        FreeCameraController camera(settings);
        auto callbacks=example.callbacks();
        callbacks.initialize=[&](Application &application)
        {
            auto &scene=application.scene();
            auto floorMaterial=application.pbrResources().createMaterial({.35f,.4f,.45f,1});
            PlaneOptions floor; floor.size={14,14}; floor.material=floorMaterial; scene.createPlane(floor);
            auto material=application.pbrResources().createMaterial({.05f,.4f,.65f,1});
            CubeOptions cube; cube.material=material; cube.size={1.2f,2,1.2f};
            scene.createCube(cube).transform.position={-1.5f,1,0};
            SphereOptions sphere; sphere.material=material; sphere.radius=.8f;
            scene.createSphere(sphere).transform.position={1.5f,1.3f,0};
            CylinderOptions pillar; pillar.material=application.pbrResources().createMaterial({.6f,.2f,.05f,1});
            pillar.radius=.4f; pillar.height=2.5f;
            scene.createCylinder(pillar).transform.position={0,1.25f,-2};
            scene.lighting().mainLight().direction={-.7f,-1,-.4f};
            scene.lighting().mainLight().intensity=3;
            scene.lighting().ambient()=glm::vec3(.08f);
            camera.apply(application.camera());
        };
        callbacks.update=[&](Application &application,float dt)
        {
            if (application.input().wasKeyPressed(Key::H))
            {
                auto &shadow=application.directionalShadow();
                if (shadow) { shadow.reset(); } else { shadow=DirectionalShadowSettings{}; }
            }
            camera.update(application.camera(),application.input(),dt,application.window().isFocused());
        };
        app.run(callbacks);
        return 0;
    }
    catch (const std::exception &error) { LOG_ERROR(error.what()); return 1; }
}
