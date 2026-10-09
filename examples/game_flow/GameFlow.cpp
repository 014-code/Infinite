#include "GameFlow.h"

#include "../common/ExampleRun.h"
#include "core/Application.h"
#include "game/menu/MenuStates.h"
#include "game/state/ApplicationStateStack.h"
#include "graphics/lighting/SceneLighting.h"
#include "physics/body/PhysicsFilter.h"
#include "physics/shapes/CollisionShape.h"
#include "save/SaveGame.h"
#include "scene/GameObject.h"
#include "scene/Scene.h"
#include "scene/components/AreaComponent.h"
#include "ui/UiCanvas.h"
#include "ui/UiLabel.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace GameFlowExample
{
    namespace
    {
        constexpr float kPlayerSpeed = 4.0f;
        constexpr float kSmokeSpeed = 5.0f;
        constexpr float kGameTimeLimit = 30.0f;
        constexpr int kSmokeMaximumFrames = 300;

        const glm::vec3 kPlayerStart{0.0f, 0.55f, 5.0f};
        const glm::vec3 kGoalPosition{0.0f, 0.65f, -5.0f};

        struct SceneBindings
        {
            // 这些ID每次SceneManager重新加载时都会更新，不能跨场景保存旧ID。
            ObjectId player = 0;
            ObjectId goal = 0;
        };

        struct SessionData
        {
            std::filesystem::path saveDirectory;
            bool smokeTest = false;
            std::shared_ptr<SceneBindings> bindings = std::make_shared<SceneBindings>();

            glm::vec3 moveInput{0.0f};
            float elapsed = 0.0f;
            bool goalReached = false;
            bool saveWritten = false;
            int smokeFrames = 0;

            // ExampleRun只在冒烟模式真正读取帧缓冲；普通运行时该回调为空操作。
            std::function<void()> verifyFrame;
        };

        using Session = SessionData;

        std::unique_ptr<ApplicationState> makeMainMenu(ApplicationStateStack &states,
            const std::shared_ptr<Session> &session);
        std::unique_ptr<ApplicationState> makeGameplay(ApplicationStateStack &states,
            const std::shared_ptr<Session> &session);
        std::unique_ptr<ApplicationState> makeResult(ApplicationStateStack &states,
            const std::shared_ptr<Session> &session);

        GameObject *findBoundObject(Application &application, ObjectId id, const char *label)
        {
            GameObject *object = application.scene().findObject(id);
            if (object == nullptr)
            {
                throw std::logic_error(std::string("Game flow scene lost ") + label + " object");
            }
            return object;
        }

        void buildGameScene(Scene &scene, const std::shared_ptr<Session> &session)
        {
            // 场景Loader只创建可序列化的几何对象；它不依赖Application，也不直接修改PhysicsWorld。
            PlaneOptions groundOptions;
            groundOptions.name = "game flow ground";
            groundOptions.size = {14.0f, 14.0f};
            groundOptions.color = {0.12f, 0.19f, 0.23f, 1.0f};
            scene.createPlane(groundOptions);

            CubeOptions playerOptions;
            playerOptions.name = "game flow player";
            playerOptions.size = {0.9f, 1.1f, 0.9f};
            playerOptions.color = {0.15f, 0.58f, 1.0f, 1.0f};
            auto &player = scene.createCube(playerOptions);
            player.transform.position = kPlayerStart;
            session->bindings->player = player.id();

            CylinderOptions goalOptions;
            goalOptions.name = "game flow goal";
            goalOptions.radius = 1.0f;
            goalOptions.height = 0.35f;
            goalOptions.radialSegments = 48;
            goalOptions.color = {0.95f, 0.52f, 0.16f, 1.0f};
            auto &goal = scene.createCylinder(goalOptions);
            goal.transform.position = kGoalPosition;
            session->bindings->goal = goal.id();

            // 简单的场景构图只属于示例层，后续可以替换成模型而不影响流程代码。
            for (int index = 0; index < 4; ++index)
            {
                CubeOptions markerOptions;
                markerOptions.name = "game flow marker " + std::to_string(index);
                markerOptions.size = {0.45f, 0.45f, 0.45f};
                markerOptions.color = {0.25f, 0.72f, 0.42f, 1.0f};
                auto &marker = scene.createCube(markerOptions);
                marker.transform.position = {
                    index % 2 == 0 ? -2.5f : 2.5f,
                    0.225f,
                    index < 2 ? 1.5f : -1.5f};
            }
        }

        void configureGameplay(Application &application, const std::shared_ptr<Session> &session)
        {
            Scene &scene = application.scene();
            PhysicsWorld &world = application.physicsWorld();

            auto *player = findBoundObject(application, session->bindings->player, "player");
            auto *goal = findBoundObject(application, session->bindings->goal, "goal");

            // 玩家使用静态体是为了展示Transform驱动物理同步；本示例不需要重力或刚体求解。
            // 真正的角色控制可以替换为CharacterBodyComponent，状态和Area逻辑不必改变。
            PhysicsFilter playerFilter;
            playerFilter.layer = PhysicsLayers::Character;
            // 当前窄相位支持球体与盒体的查询组合；视觉上仍然使用方块，
            // 这样示例不会假装物理层已经支持盒体与盒体的精确重叠。
            player->setPhysicsBody(world, CollisionShape(SphereShape(0.5f)),
                playerFilter);

            goal->setArea(world, CollisionShape(BoxShape({1.35f, 0.8f, 1.35f})),
                PhysicsLayers::Character);
            goal->area().setBodyEnteredCallback([session, playerBody = player->physicsBody().bodyId()]
                (PhysicsBodyId body)
            {
                // Area回调只记录事件，不在物理遍历中直接切换状态或清空Scene。
                if (body == playerBody)
                {
                    session->goalReached = true;
                }
            });

            scene.lighting().mainLight().direction = {-0.5f, -1.0f, -0.25f};
            scene.lighting().mainLight().color = {1.0f, 0.91f, 0.78f};
            scene.lighting().mainLight().intensity = 1.4f;
            scene.lighting().mainLight().ambient = {0.12f, 0.15f, 0.18f};
            application.camera().setView({0.0f, 8.5f, 11.0f}, {0.0f, 0.0f, 0.0f});
            application.camera().setPerspective(45.0f, 0.1f, 100.0f);
        }

        void clearGameplayUi(Application &application) noexcept
        {
            application.ui().clear();
        }

        class GameplayState final : public ApplicationState
        {
        public:
            GameplayState(ApplicationStateStack &states, std::shared_ptr<Session> session)
                : states_(&states), session_(std::move(session))
            {
            }

            void onEnter(ApplicationStateContext &context) override
            {
                auto &application = context.application();
                session_->moveInput = {};
                session_->elapsed = 0.0f;
                session_->goalReached = false;
                session_->saveWritten = false;
                session_->smokeFrames = 0;

                // 每次进入Gameplay都重新加载一份干净场景，重开关卡不会残留旧物体或旧物理体。
                application.sceneManager().load("game");
                try
                {
                    configureGameplay(application, session_);
                    buildUi(application);
                }
                catch (...)
                {
                    application.sceneManager().unload();
                    throw;
                }
            }

            void onExit(ApplicationStateContext &context) override
            {
                auto &application = context.application();
                if (session_->goalReached && !session_->saveWritten)
                {
                    SaveGameData data;
                    data.activeScene = "game";
                    data.values["completed"] = true;
                    data.values["elapsed_seconds"] = static_cast<double>(session_->elapsed);
                    SaveGameSerializer::save(application.scene(), data, session_->saveDirectory);
                    session_->saveWritten = true;
                }
                clearGameplayUi(application);
                application.sceneManager().unload();
                statusLabel_ = nullptr;
            }

            void onPause(ApplicationStateContext &context) override
            {
                // 暂停菜单暂时接管Canvas，但游戏Scene仍保留并由PauseState继续绘制。
                clearGameplayUi(context.application());
                statusLabel_ = nullptr;
            }

            void onResume(ApplicationStateContext &context) override
            {
                buildUi(context.application());
            }

            void onEvents(ApplicationStateContext &context) override
            {
                auto &application = context.application();
                if (application.input().wasKeyPressed(Key::Escape))
                {
                    context.push(std::make_unique<PauseState>(PauseActions{
                        [](ApplicationStateStack &stack) { stack.pop(); },
                        [session = session_](ApplicationStateStack &stack)
                        {
                            // 先移除Pause，再替换底下的Gameplay，避免堆积多个游戏状态。
                            stack.pop();
                            stack.replace(makeGameplay(stack, session));
                        },
                        [session = session_](ApplicationStateStack &stack)
                        {
                            stack.pop();
                            stack.replace(makeMainMenu(stack, session));
                        }}));
                }
            }

            void update(ApplicationStateContext &context, float deltaTime) override
            {
                auto &application = context.application();
                session_->elapsed = std::min(kGameTimeLimit, session_->elapsed + deltaTime);
                const auto &input = application.input();
                glm::vec3 movement{
                    static_cast<float>(input.isKeyDown(Key::D)) - static_cast<float>(input.isKeyDown(Key::A)),
                    0.0f,
                    static_cast<float>(input.isKeyDown(Key::S)) - static_cast<float>(input.isKeyDown(Key::W))};
                if (session_->smokeTest)
                {
                    const auto *player = application.scene().findObject(session_->bindings->player);
                    if (player != nullptr)
                    {
                        movement = kGoalPosition - player->transform.position;
                        movement.y = 0.0f;
                    }
                }
                if (glm::length(movement) > 1.0e-4f)
                {
                    movement = glm::normalize(movement);
                }
                session_->moveInput = movement;
                if (statusLabel_ != nullptr)
                {
                    statusLabel_->setText("WASD MOVE   ESC PAUSE   TIME " +
                        std::to_string(static_cast<int>(std::ceil(kGameTimeLimit - session_->elapsed))));
                }
            }

            void fixedUpdate(ApplicationStateContext &context, float fixedDeltaTime) override
            {
                auto &application = context.application();
                auto *player = application.scene().findObject(session_->bindings->player);
                if (player == nullptr)
                {
                    return;
                }
                player->transform.position += session_->moveInput *
                    (session_->smokeTest ? kSmokeSpeed : kPlayerSpeed) * fixedDeltaTime;
            }

            void afterPhysics(ApplicationStateContext &context, float) override
            {
                if (session_->goalReached)
                {
                    context.replace(makeResult(*states_, session_));
                }
            }

            void afterRender(ApplicationStateContext &) override
            {
                if (session_->verifyFrame)
                {
                    session_->verifyFrame();
                }
                if (session_->smokeTest && ++session_->smokeFrames > kSmokeMaximumFrames)
                {
                    throw std::runtime_error("Game flow smoke test did not reach the goal");
                }
            }

        private:
            void buildUi(Application &application)
            {
                auto &canvas = application.ui();
                canvas.clear();
                statusLabel_ = &canvas.create<UiLabel>("WASD MOVE   ESC PAUSE   TIME 30");
                statusLabel_->setRect({24.0f, 24.0f, 900.0f, 32.0f});
                statusLabel_->setScale(1.6f);
                statusLabel_->setColor({0.86f, 0.93f, 1.0f, 1.0f});
            }

            ApplicationStateStack *states_ = nullptr;
            std::shared_ptr<Session> session_;
            UiLabel *statusLabel_ = nullptr;
        };

        std::unique_ptr<ApplicationState> makeGameplay(ApplicationStateStack &states,
            const std::shared_ptr<Session> &session)
        {
            return std::make_unique<GameplayState>(states, session);
        }

        std::unique_ptr<ApplicationState> makeMainMenu(ApplicationStateStack &states,
            const std::shared_ptr<Session> &session)
        {
            auto menu = std::make_unique<MainMenuState>(MainMenuActions{
                [session, &states](ApplicationStateStack &)
                {
                    states.replace(makeGameplay(states, session));
                },
                [](ApplicationStateStack &stack) { stack.quit(); }});
            menu->setAfterRender([session, &states, frames = 0](Application &)
            mutable
            {
                if (session->verifyFrame)
                {
                    session->verifyFrame();
                }
                if (session->smokeTest && ++frames >= 2)
                {
                    states.replace(makeGameplay(states, session));
                }
            });
            return menu;
        }

        std::unique_ptr<ApplicationState> makeResult(ApplicationStateStack &states,
            const std::shared_ptr<Session> &session)
        {
            auto result = std::make_unique<ResultState>("LEVEL COMPLETE", ResultActions{
                [session, &states](ApplicationStateStack &)
                {
                    states.replace(makeGameplay(states, session));
                },
                [session, &states](ApplicationStateStack &)
                {
                    states.replace(makeMainMenu(states, session));
                }});
            result->setAfterRender([session, frames = 0](Application &application)
            mutable
            {
                if (session->verifyFrame)
                {
                    session->verifyFrame();
                }
                if (session->smokeTest && ++frames >= 2)
                {
                    application.requestClose();
                }
            });
            return result;
        }
    }

    struct GameFlowSession : SessionData
    {
    };

    std::shared_ptr<GameFlowSession> createSession(
        const std::filesystem::path &saveDirectory, bool smokeTest,
        std::function<void()> verifyFrame)
    {
        if (saveDirectory.empty())
        {
            throw std::invalid_argument("Game flow save directory must not be empty");
        }
        auto session = std::make_shared<GameFlowSession>();
        session->saveDirectory = saveDirectory;
        session->smokeTest = smokeTest;
        session->verifyFrame = std::move(verifyFrame);
        return session;
    }

    void registerScenes(Application &application,
        const std::shared_ptr<GameFlowSession> &session)
    {
        if (!session)
        {
            throw std::invalid_argument("Game flow session must not be null");
        }
        application.sceneManager().registerScene("game",
            [session](Scene &scene, ResourceManager &)
            {
                buildGameScene(scene, session);
            });
    }

    std::unique_ptr<ApplicationState> createInitialState(ApplicationStateStack &states,
        const std::shared_ptr<GameFlowSession> &session)
    {
        if (!session)
        {
            throw std::invalid_argument("Game flow session must not be null");
        }
        return makeMainMenu(states, session);
    }
}
