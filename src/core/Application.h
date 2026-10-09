#pragma once

#include "FrameTiming.h"
#include "audio/AudioSystem.h"
#include "audio/AudioEvent.h"
#include "audio/MusicPlayer.h"
#include "game/state/StateFramePolicy.h"
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
#include "physics/world/PhysicsWorld.h"
#include "scene/Scene.h"
#include "scene/SceneManager.h"
#include "ui/UiCanvas.h"
#include "ui/UiRenderer.h"

#include <functional>
#include <filesystem>
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
    // 可选的TTF/OTF字体路径。为空时UI使用内置英文5×7字库，保证旧示例无需额外资源。
    std::filesystem::path uiFontPath;
    // 音频设备失败时默认降级为Null Backend，避免没有声卡时示例和自动化测试无法启动。
    AudioConfig audio;
};

class Application;
class ApplicationStateStack;

// 回调全为可选；应用无需继承Application，也无需自己写while主循环。
struct ApplicationCallbacks
{
    // 上下文已经就绪，在此创建资源和物体。资源加载时间不计入运行时间。
    std::function<void(Application &)> initialize;
    // 收集事件后调用，暂停时也调用；适合退出等窗口级操作，不在此推进游戏时间。
    std::function<void(Application &)> onEvents;
    // 在Scene::update之前调用，可以增删物体；dt已经过暂停处理和上限限制。
    std::function<void(Application &, float)> update;
    // 在每个固定物理子步开始前调用；dt始终等于Application.physicsWorld().fixedStep()。
    // 输入应在update中采样，fixedUpdate只消费已经采样的指令，避免一帧多子步重复消费边沿事件。
    std::function<void(Application &, float)> fixedUpdate;
    // 所有固定子步完成后、Scene::update之前调用；适合同步非组件控制器的显示Transform。
    std::function<void(Application &, float)> afterPhysics;
    // 返回本帧状态对Scene、物理、渲染和音频服务的策略；未提供时全部启用。
    std::function<StateFramePolicy(Application &)> framePolicy;
    // 清屏和Scene绘制之后、交换缓冲之前调用，供截图/示例冒烟检查使用。
    std::function<void(Application &)> afterRender;
    // run结束前调用，适合状态栈释放状态对象；正常/异常路径都会尽量执行一次。
    std::function<void(Application &)> shutdown;
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
    // 使用状态栈运行应用；旧示例继续使用ApplicationCallbacks，不需要迁移。
    void run(ApplicationStateStack &states);
    void requestClose();

    Window &window();
    // 返回本应用的资源缓存。资源应在initialize中加载，并在Application关闭前使用。
    ResourceManager &resources();
    PbrResources &pbrResources() { return pbrResources_; }
    const Input &input() const;
    AudioSystem &audio();
    const AudioSystem &audio() const;
    MusicPlayer &music() noexcept { return music_; }
    AudioEventQueue &audioEvents() noexcept { return audioEvents_; }
    // 返回应用自己的动作映射。初始化时注册动作，更新回调中用它查询application.input()。
    ActionMap &actions();
    const ActionMap &actions() const;
    Scene &scene();
    SceneManager &sceneManager() noexcept { return sceneManager_; }
    const SceneManager &sceneManager() const noexcept { return sceneManager_; }
    PhysicsWorld &physicsWorld();
    const PhysicsWorld &physicsWorld() const;
    Camera &camera();
    // 返回应用自己的UI画布。Application会在场景绘制后自动绘制它；空画布不会创建GPU资源。
    UiCanvas &ui() noexcept { return ui_; }
    const UiCanvas &ui() const noexcept { return ui_; }
    // 兼容入口，返回scene().lighting().mainLight()；新功能通过Scene的lighting配置。
    DirectionalLight &directionalLight();
    const DirectionalLight &directionalLight() const;
    const FrameTime &frameTime() const;
    // 可在update中开关/调整覆盖盒，下一帧生效；设置值在渲染前验证。
    std::optional<DirectionalShadowSettings> &directionalShadow() { return config_.directionalShadow; }

private:
    // 处理一帧开始阶段的窗口和输入事件，并在所有可能影响场景的事件完成后
    // 读取本帧策略。返回false表示窗口已经请求关闭，主循环应立即结束。
    bool processEvents(const ApplicationCallbacks &callbacks, bool paused,
        StateFramePolicy &policy);
    // 推进一帧的游戏逻辑、固定物理子步、场景服务和音频服务。
    // 物理同步顺序在这里集中维护，避免主循环和其他入口产生不同的模拟规则。
    void updateFrame(const ApplicationCallbacks &callbacks,
        const StateFramePolicy &policy, float deltaTime);
    // 根据当前策略绘制场景和UI，并在最后检查OpenGL错误、交换窗口缓冲区。
    // framebufferSize由主循环在事件阶段后读取，确保本帧渲染使用最新尺寸。
    void renderFrame(const ApplicationCallbacks &callbacks,
        const StateFramePolicy &policy, const glm::ivec2 &framebufferSize);

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
    AudioSystem audio_;
    MusicPlayer music_{audio_};
    AudioEventQueue audioEvents_;
    ActionMap actions_;
    Renderer renderer_;
    Camera camera_;
    UiCanvas ui_;
    // UI渲染器延迟到第一次真正有控件需要绘制时创建，避免空白Application额外编译Shader。
    std::unique_ptr<UiRenderer> uiRenderer_;
    // 声明在Scene之前，析构时Scene会先销毁并注销其物理组件，最后才销毁物理世界。
    PhysicsWorld physicsWorld_;
    Scene scene_{primitiveResources_, resources_};
    SceneManager sceneManager_{scene_, resources_};
    bool started_ = false;
};
