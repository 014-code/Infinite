#include "MiniGameScene.h"

#include "../common/CameraRelativeInput.h"
#include "core/Application.h"
#include "core/Log.h"
#include "graphics/resources/Material.h"
#include "physics/body/PhysicsFilter.h"
#include "physics/character/CharacterController.h"
#include "physics/shapes/CollisionShape.h"
#include "physics/world/PhysicsWorld.h"
#include "resources/Model.h"
#include "scene/ModelInstantiator.h"
#include "scene/Scene.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace MiniGameExample
{
    namespace
    {
        const glm::vec3 kUp{0.0f, 1.0f, 0.0f};
        const glm::quat kNoRotation{1.0f, 0.0f, 0.0f, 0.0f};
        constexpr float kCharacterSpeed = 4.5f;
        constexpr float kCharacterSprintSpeed = 7.0f;
        constexpr float kJumpSpeed = 6.0f;
        constexpr float kCameraDistance = 7.5f;
        constexpr float kCameraHeight = 2.4f;
        constexpr float kMouseSensitivity = 0.0025f;
        constexpr float kPickupDistance = 1.25f;
        constexpr float kGameDuration = 90.0f;

        const glm::vec3 kPlayerStart{0.0f, 2.0f, 0.0f};
        const std::vector<glm::vec3> kPickupPositions{
            {-6.0f, 1.0f, -5.0f}, {-1.5f, 1.0f, -6.0f}, {5.5f, 1.0f, -4.0f},
            {-5.5f, 1.0f, 4.5f}, {0.5f, 1.0f, 5.5f}, {6.0f, 1.0f, 4.0f}};

        struct Pickup
        {
            ModelInstance instance;
            glm::vec3 position{0.0f};
        };

        struct GameStateData
        {
            PhysicsWorld *world = nullptr;
            ObjectId playerId = 0;
            std::shared_ptr<Model> pickupModel;
            std::vector<glm::vec3> pickupPositions = kPickupPositions;
            std::vector<Pickup> pickups;
            int collected = 0;
            float timeRemaining = kGameDuration;
            bool finished = false;
            bool jumpRequested = false;
            bool resetRequested = false;
            bool playerResetApplied = false;
            bool looking = false;
            float cameraYaw = glm::radians(-135.0f);
            float cameraPitch = glm::radians(28.0f);
            float characterYaw = 0.0f;
        };

        void registerActions(ActionMap &actions)
        {
            // 动作名属于示例层；同一套引擎可以由其他游戏注册不同的按键方案。
            actions.createAction("mini_move_forward");
            actions.bindKey("mini_move_forward", Key::W);
            actions.createAction("mini_move_backward");
            actions.bindKey("mini_move_backward", Key::S);
            actions.createAction("mini_move_left");
            actions.bindKey("mini_move_left", Key::A);
            actions.createAction("mini_move_right");
            actions.bindKey("mini_move_right", Key::D);
            actions.createAction("mini_jump");
            actions.bindKey("mini_jump", Key::Space);
            actions.createAction("mini_sprint");
            actions.bindKey("mini_sprint", Key::LeftShift);
            actions.bindKey("mini_sprint", Key::RightShift);
        }

        void createTerrain(Scene &scene, PhysicsWorld &world)
        {
            PhysicsFilter terrain;
            terrain.layer = PhysicsLayers::Terrain;

            // 可见地面与无限物理平面分开创建：前者是渲染对象，后者是碰撞服务中的形状。
            PlaneOptions groundOptions;
            groundOptions.name = "arena ground";
            groundOptions.size = {24.0f, 24.0f};
            groundOptions.color = {0.16f, 0.22f, 0.28f, 1.0f};
            auto &ground = scene.createPlane(groundOptions);
            ground.transform.position.y = -0.02f;
            world.createStaticBody(CollisionShape(PlaneShape(kUp, 0.0f)),
                glm::vec3(0.0f), kNoRotation, terrain);

            // 这些方块既是场景装饰，也是玩家真正可以绕行的静态碰撞体。
            const std::vector<std::pair<glm::vec3, glm::vec3>> obstacles{
                {{-2.5f, 1.0f, -2.0f}, {2.0f, 2.0f, 2.0f}},
                {{2.5f, 0.75f, -1.0f}, {1.5f, 1.5f, 1.5f}},
                {{0.0f, 1.0f, 3.0f}, {2.5f, 2.0f, 1.2f}},
                {{-4.5f, 0.75f, 1.0f}, {1.2f, 1.5f, 2.4f}}};
        for (std::size_t index = 0; index < obstacles.size(); ++index)
        {
            CubeOptions options;
            options.name = "arena obstacle " + std::to_string(index);
            options.size = obstacles[index].second;
            options.color = {0.34f + static_cast<float>(index) * 0.04f,
                0.38f, 0.48f + static_cast<float>(index) * 0.03f, 1.0f};
            auto &obstacle = scene.createCube(options);
            obstacle.transform.position = obstacles[index].first;
            obstacle.setPhysicsBody(world, CollisionShape(BoxShape(options.size * 0.5f)), terrain);
        }

        // 中央平台只用于构图，不挡住玩家出生点；它也让收集物之间有明显的空间层次。
        CylinderOptions platformOptions;
        platformOptions.name = "central platform";
        platformOptions.radius = 2.2f;
        platformOptions.height = 0.35f;
        platformOptions.radialSegments = 48;
        platformOptions.color = {0.30f, 0.34f, 0.40f, 1.0f};
        auto &platform = scene.createCylinder(platformOptions);
        platform.transform.position.y = 0.175f;
        platform.setPhysicsBody(world, CollisionShape(BoxShape({2.2f, 0.175f, 2.2f})), terrain);
        }

        GameObject &createPlayer(Scene &scene, PhysicsWorld &world)
        {
            const CharacterSettings settings{0.42f, 1.0f};
            CylinderOptions playerOptions;
            playerOptions.name = "player";
            playerOptions.radius = settings.radius;
            playerOptions.height = settings.cylinderHeight + settings.radius * 2.0f;
            playerOptions.radialSegments = 32;
            playerOptions.color = {0.18f, 0.62f, 1.0f, 1.0f};
            auto &player = scene.createCylinder(playerOptions);
            player.transform.position = kPlayerStart;
            player.setCharacterBody(world, settings, PhysicsLayers::Terrain);
            return player;
        }

        void applyCamera(Application &application, const GameStateData &state);

        void spawnPickups(Application &application, GameStateData &state)
        {
            Scene &scene = application.scene();
            state.pickups.clear();
            state.pickups.reserve(state.pickupPositions.size());
            for (std::size_t index = 0; index < state.pickupPositions.size(); ++index)
            {
                auto instance = ModelInstantiator::instantiate(scene, *state.pickupModel,
                    "pickup/avocado_" + std::to_string(index));
                GameObject *root = scene.findObject(instance.rootId);
                if (root == nullptr)
                {
                    throw std::logic_error("Mini game pickup root disappeared during creation");
                }
                root->transform.position = state.pickupPositions[index];
                // 模型来自外部资源，旋转只属于实例Transform，不会修改共享Model资源。
                root->transform.scale = glm::vec3(8.0f);
                root->script().setUpdateCallback([](GameObject &object, float deltaTime)
                {
                    object.transform.rotateEuler({0.0f, deltaTime * 1.4f, 0.0f});
                });
                state.pickups.push_back(Pickup{std::move(instance), state.pickupPositions[index]});
            }
        }

        void removePickups(Scene &scene, GameStateData &state)
        {
            for (auto &pickup : state.pickups)
            {
                ModelInstantiator::remove(scene, pickup.instance);
            }
            state.pickups.clear();
        }

        void resetGame(Application &application, GameStateData &state)
        {
            removePickups(application.scene(), state);
            spawnPickups(application, state);
            state.collected = 0;
            state.timeRemaining = kGameDuration;
            state.finished = false;
            state.jumpRequested = false;
            state.resetRequested = false;
            state.playerResetApplied = false;
            state.characterYaw = 0.0f;
            if (GameObject *player = application.scene().findObject(state.playerId))
            {
                auto &body = player->characterBody();
                body.state().position = kPlayerStart;
                body.state().velocity = glm::vec3(0.0f);
                body.state().grounded = false;
                player->transform.position = kPlayerStart;
                player->transform.setRotation(kNoRotation);
            }
            LOG_INFO("mini_game reset: collect all energy avocados");
        }
    }

    struct GameState : GameStateData
    {
        // 具体字段集中在私有实现状态中，公共头文件只暴露不透明句柄，避免玩法细节扩散到示例入口。
    };

    std::shared_ptr<GameState> createScene(Application &application,
        const std::filesystem::path &directory)
    {
        auto state = std::make_shared<GameState>();
        state->world = &application.physicsWorld();
        registerActions(application.actions());

        auto &lighting = application.scene().lighting();
        lighting.ambient() = {0.14f, 0.17f, 0.22f};
        lighting.mainLight().direction = {-0.5f, -1.0f, -0.35f};
        lighting.mainLight().color = {0.86f, 0.92f, 1.0f};
        lighting.mainLight().intensity = 0.85f;
        // 本示例Shader只实现基础方向光，因此不配置额外点光源。
        // 需要点光/聚光的完整上传路径请运行scene_lighting示例。

        createTerrain(application.scene(), *state->world);
        auto &player = createPlayer(application.scene(), *state->world);
        state->playerId = player.id();

        // 该模型来自Khronos glTF Sample Assets，示例目录中的SOURCE.md记录CC0来源。
        // 只加载一次，之后的六个收集物实例共享Mesh和Material等GPU资源。
        state->pickupModel = application.resources().loadModel(
            directory / "assets/avocado.glb",
            directory / "shaders/game.vert",
            directory / "shaders/game.frag");
        spawnPickups(application, *state);

        application.camera().setPerspective(55.0f, 0.1f, 100.0f);
        applyCamera(application, *state);
        LOG_INFO("mini_game ready: WASD move, Shift sprint, Space jump, RMB look, R reset");
        return state;
    }

    void update(Application &application, GameState &state, float deltaTime)
    {
        const Input &input = application.input();
        if (input.isMouseButtonDown(MouseButton::Right) && application.window().isFocused())
        {
            if (state.looking)
            {
                const glm::dvec2 mouseDelta = input.mouseDelta();
                state.cameraYaw = static_cast<float>(std::remainder(
                    double(state.cameraYaw) + mouseDelta.x * kMouseSensitivity, glm::two_pi<double>()));
                state.cameraPitch = std::clamp(state.cameraPitch -
                    static_cast<float>(mouseDelta.y * kMouseSensitivity), -0.1f, 1.0f);
            }
            state.looking = true;
        }
        else
        {
            state.looking = false;
        }

        if (application.window().isFocused() && input.wasKeyPressed(Key::R))
        {
            state.resetRequested = true;
        }
        state.jumpRequested = state.jumpRequested ||
            application.actions().wasPressed("mini_jump", input);
        if (!state.finished)
        {
            state.timeRemaining = std::max(0.0f, state.timeRemaining - deltaTime);
            if (state.timeRemaining <= 0.0f)
            {
                state.finished = true;
                LOG_INFO("mini_game time up: press R to try again");
            }
        }
    }

    void fixedUpdate(Application &application, GameState &state, float fixedDeltaTime)
    {
        GameObject *player = application.scene().findObject(state.playerId);
        if (player == nullptr || !player->characterBody().isAttached())
        {
            return;
        }
        auto &body = player->characterBody();
        auto &character = body.state();
        if (state.resetRequested && !state.playerResetApplied)
        {
            character.position = kPlayerStart;
            character.velocity = glm::vec3(0.0f);
            character.grounded = false;
            state.playerResetApplied = true;
            // 这里故意保留resetRequested，等afterPhysics统一重建收集物和计时状态。
            // 这样即使本帧已经结束，按R也能走完整复位流程。
        }
        if (state.finished)
        {
            return;
        }

        const Input &input = application.input();
        const float forward = float(application.actions().isDown("mini_move_forward", input)) -
            float(application.actions().isDown("mini_move_backward", input));
        const float right = float(application.actions().isDown("mini_move_right", input)) -
            float(application.actions().isDown("mini_move_left", input));
        const glm::vec3 cameraTarget = character.position + glm::vec3(0.0f, 0.65f, 0.0f);
        glm::vec3 direction = cameraRelativeDirection(cameraTarget - application.camera().position(), forward, right);
        const float speed = application.actions().isDown("mini_sprint", input)
            ? kCharacterSprintSpeed : kCharacterSpeed;
        character.velocity.x = 0.0f;
        character.velocity.z = 0.0f;
        if (glm::length(direction) > 0.0f)
        {
            direction = glm::normalize(direction);
            character.velocity.x = direction.x * speed;
            character.velocity.z = direction.z * speed;
            state.characterYaw = std::atan2(direction.x, direction.z);
        }
        character.velocity.y += state.world->gravity().y * fixedDeltaTime;
        if (character.grounded && character.velocity.y <= 0.0f)
        {
            character.velocity.y = -1.0f;
        }
        if (state.jumpRequested && character.grounded)
        {
            character.velocity.y = kJumpSpeed;
        }
        state.jumpRequested = false;
        body.move(fixedDeltaTime);
    }

    void afterPhysics(Application &application, GameState &state, float interpolationAlpha)
    {
        (void)interpolationAlpha;
        if (state.resetRequested)
        {
            // 没有固定子步时也能完成R复位，避免低帧率或暂停首帧留下旧位置。
            resetGame(application, state);
        }

        GameObject *player = application.scene().findObject(state.playerId);
        if (player != nullptr && !state.finished)
        {
            for (std::size_t index = state.pickups.size(); index > 0; --index)
            {
                Pickup &pickup = state.pickups[index - 1];
                if (glm::distance(player->transform.position, pickup.position) > kPickupDistance)
                {
                    continue;
                }
                ModelInstantiator::remove(application.scene(), pickup.instance);
                state.pickups.erase(state.pickups.begin() + static_cast<std::ptrdiff_t>(index - 1));
                ++state.collected;
                LOG_INFO("mini_game collected " + std::to_string(state.collected) + "/" +
                    std::to_string(state.pickupPositions.size()));
            }
            if (state.pickups.empty())
            {
                state.finished = true;
                LOG_INFO("mini_game victory: all energy avocados collected");
            }
        }

        applyCamera(application, state);
        if (player != nullptr)
        {
            player->transform.setRotation(glm::quat(glm::vec3(0.0f, state.characterYaw, 0.0f)));
        }
    }

    namespace
    {
        void applyCamera(Application &application, const GameStateData &state)
        {
            const GameObject *player = application.scene().findObject(state.playerId);
            const glm::vec3 target = player == nullptr
                ? kPlayerStart + glm::vec3(0.0f, 0.65f, 0.0f)
                : player->transform.position + glm::vec3(0.0f, 0.65f, 0.0f);
            const float horizontal = std::cos(state.cameraPitch);
            const glm::vec3 offset(std::cos(state.cameraYaw) * horizontal,
                std::sin(state.cameraPitch), std::sin(state.cameraYaw) * horizontal);
            application.camera().setView(target + offset * kCameraDistance + kUp * kCameraHeight,
                target, kUp);
        }
    }
}
