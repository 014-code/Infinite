#pragma once

#include "../common/FreeCameraController.h"

class Application;

namespace PrimitiveExample
{
    // 示例和截图测试复用同一份布局；不让框架依赖示例的相机、颜色或旋转策略。
    FreeCameraSettings cameraSettings();
    void createScene(Application &application);
}
