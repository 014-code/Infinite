#include "SceneLightingScene.h"

#include "core/Application.h"

namespace SceneLightingExample
{
    FreeCameraSettings cameraSettings()
    {
        FreeCameraSettings settings;
        settings.startPosition = {7.0f, 4.4f, 9.0f};
        settings.startYaw = -2.25f;
        settings.startPitch = -0.38f;
        return settings;
    }

    void createScene(Application &application)
    {
        // 示例层只决定“放什么”和“灯放在哪里”；网格、材质和光照计算仍由框架负责。
        PlaneOptions ground;
        ground.name = "ground";
        ground.size = {12.0f, 12.0f};
        ground.color = {0.4f, 0.42f, 0.46f, 1.0f};
        auto &floor = application.scene().createPlane(ground);
        floor.transform.position.y = -1.0f;

        CubeOptions cube;
        cube.name = "cube";
        // 偏中性色材质便于辨别灯光颜色，避免红物体天然滤掉蓝灯而误认为灯没工作。
        cube.color = {0.85f, 0.72f, 0.62f, 1.0f};
        auto &redCube = application.scene().createCube(cube);
        redCube.transform.position = {-2.2f, 0.0f, 0.0f};
        redCube.transform.setEulerAngles({0.2f, -0.3f, 0.0f});

        SphereOptions sphere;
        sphere.name = "sphere";
        sphere.color = {0.65f, 0.72f, 0.85f, 1.0f};
        sphere.radius = 1.0f;
        auto &blueSphere = application.scene().createSphere(sphere);
        blueSphere.transform.position = {0.0f, 0.0f, 0.0f};

        CylinderOptions cylinder;
        cylinder.name = "cylinder";
        cylinder.color = {0.65f, 0.8f, 0.68f, 1.0f};
        cylinder.radius = 0.75f;
        cylinder.height = 2.0f;
        cylinder.radialSegments = 48;
        auto &greenCylinder = application.scene().createCylinder(cylinder);
        greenCylinder.transform.position = {2.2f, 0.0f, 0.0f};

        greenCylinder.script().setUpdateCallback([](GameObject &object, float deltaTime)
        {
            object.transform.rotateEuler({0.0f, deltaTime * 0.6f, 0.0f});
        });

        auto &lighting = application.scene().lighting();
        lighting.ambient() = {0.07f, 0.08f, 0.1f};
        lighting.mainLight().direction = {-0.5f, -1.0f, -0.35f};
        lighting.mainLight().color = {0.85f, 0.9f, 1.0f};
        lighting.mainLight().intensity = 0.25f;
        lighting.additionalDirectionalLights.push_back({{0, -0.4f, 1}, {1, .85f, .7f}, .15f});

        // 左侧暖色点光，让用户能直观看到距离衰减和物体法线的差异。
        lighting.pointLights.push_back({{-2.8f, 2.4f, 1.0f}, {1.0f, 0.34f, 0.12f}, 12.0f, 7.0f});
        // 右侧冷色聚光灯朝向场景中心，外锥区域会逐渐变暗。
        SpotLight spot;
        spot.position = {3.2f, 3.4f, 2.0f};
        spot.direction = {-0.55f, -0.75f, -0.25f};
        spot.color = {0.2f, 0.45f, 1.0f};
        spot.intensity = 24.0f;
        spot.range = 9.0f;
        spot.innerAngle = 0.24f;
        spot.outerAngle = 0.56f;
        lighting.spotLights.push_back(spot);

        application.camera().setPerspective(45.0f, 0.1f, 100.0f);
        FreeCameraController(cameraSettings()).apply(application.camera());
    }
}
