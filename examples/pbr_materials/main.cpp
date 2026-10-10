#include "PbrScene.h"
#include "common/ExampleRun.h"
#include <iostream>

int main(int argc, char *argv[])
{
    try
    {
        ExampleRun example(argc, argv);
        const auto directory = executableDirectory(argv[0]);
        example.beginLogging(directory, "pbr_materials");
        ApplicationConfig config;
        config.title = "Infinite - PBR | rows: dielectric / metal; columns: roughness | WASD / RMB";
        config.visible = example.visible();
        config.linearHdr = true; // 光照/混合在线性RGBA16F中完成，再统一输出到窗口。
        Application application(config);
        FreeCameraController camera(PbrExample::cameraSettings());
        auto callbacks = example.callbacks();
        callbacks.initialize = [](Application &app) { PbrExample::createScene(app); };
        callbacks.update = [&](Application &app, float dt)
        { camera.update(app.camera(), app.input(), dt, app.window().isFocused()); };
        application.run(callbacks);
        example.finishLogging("pbr_materials");
        return 0;
    }
    catch (const std::exception &error)
    {
        LOG_ERROR(error.what());
        return 1;
    }
}
