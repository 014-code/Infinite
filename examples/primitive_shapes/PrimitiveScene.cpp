#include "PrimitiveScene.h"
#include "core/Application.h"

namespace PrimitiveExample
{
    FreeCameraSettings cameraSettings()
    {
        FreeCameraSettings settings;
        settings.startPosition = {7.0f, 4.5f, 9.0f};
        settings.startYaw = -2.25f;
        settings.startPitch = -0.4f;
        settings.moveSpeed = 5.0f;
        return settings;
    }

    void createScene(Application &application)
    {
        // 这些物体全部由框架生成；示例只负责选择形状、颜色和摆放位置。
        PlaneOptions groundOptions;
        groundOptions.name = "ground";
        groundOptions.color = {0.32f, 0.38f, 0.46f, 1.0f};
        groundOptions.size = {10.0f, 10.0f};
        auto &ground = application.scene().createPlane(groundOptions);
        ground.transform.position.y = -1.0f;

        // 不提供参数也可以直接生成：默认尺寸和灰色材质由引擎准备。
        auto &cube = application.scene().createPrimitive(PrimitiveType::Cube);
        cube.transform.scale = glm::vec3(1.6f);
        cube.transform.position = {-3.0f, 0.0f, 0.0f};
        cube.transform.setEulerAngles({0.2f, -0.35f, 0.0f});

        SphereOptions sphereOptions;
        sphereOptions.name = "sphere";
        sphereOptions.color = {0.16f, 0.65f, 0.98f, 1.0f};
        sphereOptions.radius = 0.95f;
        auto &sphere = application.scene().createSphere(sphereOptions);
        sphere.transform.position = {-1.0f, 0.0f, 0.0f};

        CylinderOptions cylinderOptions;
        cylinderOptions.name = "cylinder";
        cylinderOptions.color = {0.95f, 0.5f, 0.12f, 1.0f};
        cylinderOptions.radius = 0.8f;
        cylinderOptions.height = 2.0f;
        cylinderOptions.radialSegments = 48;
        auto &cylinder = application.scene().createCylinder(cylinderOptions);
        cylinder.transform.position = {1.1f, 0.0f, 0.0f};

        ConeOptions coneOptions;
        coneOptions.name = "cone";
        coneOptions.color = {0.98f, 0.32f, 0.12f, 1.0f};
        coneOptions.radius = 0.9f;
        coneOptions.height = 2.0f;
        coneOptions.radialSegments = 48;
        auto &cone = application.scene().createCone(coneOptions);
        cone.transform.position = {3.2f, 0.0f, 0.0f};

        DiskOptions diskOptions;
        diskOptions.name = "disk";
        diskOptions.color = {0.22f, 0.85f, 0.35f, 1.0f};
        diskOptions.radius = 0.85f;
        diskOptions.radialSegments = 48;
        auto &disk = application.scene().createDisk(diskOptions);
        disk.transform.position = {0.0f, 1.65f, 0.0f};
        disk.transform.setEulerAngles({glm::half_pi<float>(), 0.0f, 0.0f});

        // 旋转几个物体，让运行时能同时观察圆周分段、法线和深度遮挡。
        cylinder.setUpdateCallback([](GameObject &object, float deltaTime)
        {
            object.transform.rotateEuler({0.0f, deltaTime * 0.55f, 0.0f});
        });
        cone.setUpdateCallback([](GameObject &object, float deltaTime)
        {
            object.transform.rotateEuler({0.0f, -deltaTime * 0.35f, 0.0f});
        });

        auto &light = application.directionalLight();
        light.direction = {-0.6f, -1.0f, -0.8f};
        light.color = {1.0f, 0.96f, 0.9f};
        light.intensity = 0.9f;
        light.ambient = {0.16f, 0.18f, 0.22f};
        application.camera().setPerspective(45.0f, 0.1f, 100.0f);
        FreeCameraController(cameraSettings()).apply(application.camera());
    }
}
