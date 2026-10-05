#pragma once

#include "../common/FreeCameraController.h"

class Application;

namespace SceneLightingExample
{
    // 示例和截图测试共用初始镜头，R复位与首次打开的构图保持一致。
    FreeCameraSettings cameraSettings();
    void createScene(Application &application);
}
