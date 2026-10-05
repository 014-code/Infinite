#pragma once

#include "../common/FreeCameraController.h"
#include "core/Application.h"
#include "scene/ModelInstantiator.h"

#include <array>
#include <filesystem>

namespace MaterialShowcase
{
    // 展示对象保存模型实例句柄，不保存模型文件指针；GPU资源由Scene中的Renderable共享持有。
    struct Exhibits
    {
        std::array<ModelInstance, 3> instances;
        ObjectId platformId = 0; // 平台独立于模型实例，便于应用查找及回归测试。
        bool rotating = true;
    };

    FreeCameraSettings cameraSettings();
    Exhibits createScene(Application &application, const std::filesystem::path &directory);
    void updateExhibits(Application &application, const Exhibits &exhibits, float deltaTime);
}
