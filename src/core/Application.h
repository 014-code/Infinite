#pragma once

#include "FrameTiming.h"
#include "graphics/camera/Camera.h"
#include "graphics/lighting/Lighting.h"
#include "graphics/rendering/Renderer.h"
#include "input/ActionMap.h"
#include "input/Input.h"
#include "platform/Window.h"
#include "resources/ResourceManager.h"
#include "resources/PrimitiveResources.h"
#include "resources/PbrResources.h"
#include "graphics/rendering/HdrPipeline.h"
#include "graphics/rendering/shadows/DirectionalShadowMap.h"
#include "scene/Scene.h"

#include <functional>
#include <optional>
#include <string>
#include <glm/vec4.hpp>

struct ApplicationConfig
{
    int width = 1280;
    int height = 800;
    std::string title = "Infinite";
    bool visible = true;
    float maximumDeltaTime = 0.1f;
    glm::vec4 clearColor{0.035f, 0.045f, 0.07f, 1.0f};
    bool linearHdr = false; // 新PBR场景显式启用；旧示例不改变颜色输出行为。
    float exposure = 1.0f;
    std::optional<DirectionalShadowSettings> directionalShadow; // 首版需要linearHdr和PBR材质。
};

class Application;

// 回调全为可选；应用无需继承Application，也无需自己写while主循环。
struct ApplicationCallbacks
{
    // 上下文已经就绪，在此创建资源和物体。资源加载时间不计入运行时间。
    std::function<void(Application &)> initialize;
    // 收集事件后调用，暂停时也调用；适合退出等窗口级操作，不在此推进游戏时间。
    std::function<void(Application &)> onEvents;
    // 在Scene::update之前调用，可以增删物体；dt已经过暂停处理和上限限制。
    std::function<void(Application &, float)> update;
    // 清屏和Scene绘制之后、交换缓冲之前调用，供截图/示例冒烟检查使用。
    std::function<void(Application &)> afterRender;
};

// 应用运行层只组装通用模块，不包含示例形状或按键方案；资源缓存由ResourceManager提供。
// 只能在主线程使用；不要让GPU资源或持有它们的回调活得比Application更久。
class Application final
{
public:
    explicit Application(const ApplicationConfig &config = {});
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(Application &&) = delete;

    // 仅能运行一次。正常/异常退出均清空Scene，异常继续交给应用入口记录。
    // 回调仅在run期间借用，不存进Application；执行中不要修改这组回调本身。
    void run(const ApplicationCallbacks &callbacks = {});
    void requestClose();

    Window &window();
    // 返回本应用的资源缓存。资源应在initialize中加载，并在Application关闭前使用。
    ResourceManager &resources();
    PbrResources &pbrResources() { return pbrResources_; }
    const Input &input() const;
    // 返回应用自己的动作映射。初始化时注册动作，更新回调中用它查询application.input()。
    ActionMap &actions();
    const ActionMap &actions() const;
    Scene &scene();
    Camera &camera();
    // 兼容入口，返回scene().lighting().mainLight()；新功能通过Scene的lighting配置。
    DirectionalLight &directionalLight();
    const DirectionalLight &directionalLight() const;
    const FrameTime &frameTime() const;
    // 可在update中开关/调整覆盖盒，下一帧生效；设置值在渲染前验证。
    std::optional<DirectionalShadowSettings> &directionalShadow() { return config_.directionalShadow; }

private:
    // C++成员按声明顺序构造、逆序销毁：先释放Scene持有的GPU资源，最后关闭Window。
    // 配置和时间参数先校验，非法配置不会创建窗口。
    ApplicationConfig config_;
    FrameTiming timing_;
    Window window_;
    // 声明在Window之后，析构时会先于Window释放Shader和Texture的GPU资源。
    ResourceManager resources_;
    // 内置几何资源也是按需创建，先于Scene声明，确保Scene先释放其持有关系。
    PrimitiveResources primitiveResources_;
    PbrResources pbrResources_;
    HdrPipeline hdrPipeline_;
    DirectionalShadowMap shadowMap_;
    Input input_;
    ActionMap actions_;
    Renderer renderer_;
    Camera camera_;
    Scene scene_{primitiveResources_, resources_};
    bool started_ = false;
};
