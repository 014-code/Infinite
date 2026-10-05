#include "PbrScene.h"
#include "graphics/resources/Material.h"

namespace PbrExample
{
    FreeCameraSettings cameraSettings()
    {
        FreeCameraSettings settings;
        settings.startPosition = {0, 2.5f, 11};
        settings.startYaw = glm::radians(-90.0f);
        settings.startPitch = glm::radians(-8.0f);
        return settings;
    }

    void createScene(Application &application)
    {
        // 上排金属，下排非金属；从左到右粗糙度增大。只改变表面参数，不更换光源。
        for (int row = 0; row < 2; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                PbrParameters pbr;
                pbr.metallic = static_cast<float>(row);
                pbr.roughness = .08f + column * .22f;
                SphereOptions options;
                options.radius = .65f;
                options.radialSegments = 64;
                options.latitudeSegments = 32;
                options.material = application.pbrResources().createMaterial({.7f, .3f, .08f, 1}, pbr);
                auto &sphere = application.scene().createSphere(options);
                sphere.transform.position = {(column - 2) * 1.6f, row * 1.7f, 0};
            }
        }
        PlaneOptions floor;
        floor.size = {11, 7};
        floor.material = application.pbrResources().createMaterial({.12f, .15f, .19f, 1});
        application.scene().createPlane(floor).transform.position.y = -.7f;

        PbrParameters glowing;
        glowing.emission = {3, .3f, .04f};
        CubeOptions marker;
        marker.size = {.3f, .3f, .3f};
        marker.material = application.pbrResources().createMaterial({0, 0, 0, 1}, glowing);
        application.scene().createCube(marker).transform.position = {0, 3.1f, 0};
        auto &lighting = application.scene().lighting();
        lighting.ambient() = {.03f, .03f, .03f};
        lighting.mainLight().direction = {-.4f, -.7f, -1};
        lighting.mainLight().intensity = 3;
        lighting.pointLights.push_back({{3, 3, 3}, {.25f, .5f, 1}, 45, 12});
        application.camera().setPerspective(45, .1f, 100);
        FreeCameraController(cameraSettings()).apply(application.camera());
    }
}
