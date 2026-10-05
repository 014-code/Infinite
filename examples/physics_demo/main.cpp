// 物理系统示例：静态地形 + 角色控制器 + 动态刚体球 + 射线放置 + 碰撞体可视化。
//
// 本例把物理世界放在Application之外持有：PhysicsWorld必须比Scene活得更久，否则
// Scene销毁时物体的PhysicsBodyComponent会向已销毁的世界注销句柄。main里的
// shared_ptr在runExample返回（Application析构）之后才释放，满足这个顺序。
#include "common/ExampleRun.h"
#include "common/CameraRelativeInput.h"

#include "core/Application.h"
#include "graphics/resources/Material.h"
#include "physics/body/PhysicsFilter.h"
#include "physics/character/CharacterController.h"
#include "physics/world/PhysicsWorld.h"
#include "scene/Scene.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
    const glm::vec3 UP(0.0f, 1.0f, 0.0f);
    const glm::quat NO_ROTATION(1.0f, 0.0f, 0.0f, 0.0f);
    constexpr float BALL_RADIUS = 0.3f;
    constexpr float PLACED_BOX_HALF = 0.25f;
    constexpr float CHARACTER_SPEED = 4.5f;
    constexpr float CHARACTER_SPRINT = 8.0f;
    constexpr float CHARACTER_JUMP_SPEED = 6.0f;
    constexpr float CAMERA_DISTANCE = 6.5f;
    constexpr float MOUSE_SENSITIVITY = 0.0025f;

    // 一个动态球的可视物体与物理体配对，便于每帧同步位姿。
    struct BallVisual
    {
        PhysicsBodyId body = 0;
        ObjectId object = 0;
    };

    // 调试叠加：plane的位姿固定，不随物理体（永远在世界原点）移动。
    struct DebugVisual
    {
        PhysicsBodyId body = 0;
        ObjectId object = 0;
        bool fixedPose = false;
    };

    struct DemoState
    {
        std::shared_ptr<PhysicsWorld> world;
        CharacterController controller{CharacterSettings{0.35f, 1.0f}};
        CharacterState character;
        ObjectId characterVisual = 0;

        std::vector<BallVisual> balls;
        std::vector<ObjectId> placedBoxes;

        // 调试可视化：开关打开时按物理体生成半透明形状叠加。
        // 半透明材质由示例持有，借用给渲染组件；销毁叠加物体后才能释放。
        bool debugVisible = false;
        std::vector<DebugVisual> debugVisuals;
        // 叠加用的半透明材质由示例持有；必须先删除借用它们的物体再释放材质。
        std::vector<std::shared_ptr<Material>> debugMaterials;
        // 玩家可交互的物体统一放在Prop层，地形放在Terrain层。
        PhysicsFilter propFilter{PhysicsLayers::Prop, 0xFFFFFFFFu};
        std::uint32_t characterMask = PhysicsLayers::Terrain | PhysicsLayers::Prop | PhysicsLayers::Default;

        float cameraYaw = -glm::half_pi<float>();
        float cameraPitch = 0.35f;
        bool looking = false;
        float characterYaw = 0.0f;
        unsigned int spawnCounter = 0;
    };

    // 复制内置几何体材质并改成半透明，避免示例自己维护Shader。
    std::shared_ptr<Material> makeTranslucentMaterial(const Renderable &source, const glm::vec4 &color)
    {
        auto material = std::make_shared<Material>(*source.material());
        material->setRenderMode(RenderMode::AlphaBlend);
        material->setCullMode(CullMode::None);
        material->setBaseColor(color);
        return material;
    }

    // 静态地形：一块无限地面、一段可行走斜坡、一段陡坡、几级台阶和两个可被射线命中的方块。
    void createTerrain(Scene &scene, PhysicsWorld &world)
    {
        PhysicsFilter terrain;
        terrain.layer = PhysicsLayers::Terrain;

        PlaneOptions groundOptions;
        groundOptions.name = "ground";
        groundOptions.color = glm::vec4(0.35f, 0.42f, 0.36f, 1.0f);
        groundOptions.size = glm::vec2(40.0f);
        scene.createPlane(groundOptions);
        world.createStaticBody(CollisionShape(PlaneShape(UP, 0.0f)), glm::vec3(0.0f), NO_ROTATION, terrain);

        // 20度斜坡：可站立，用于演示沿着表面向上/向下滑动。
        CubeOptions rampOptions;
        rampOptions.name = "ramp";
        rampOptions.color = glm::vec4(0.45f, 0.5f, 0.62f, 1.0f);
        rampOptions.size = glm::vec3(4.0f, 0.4f, 6.0f);
        GameObject &ramp = scene.createCube(rampOptions);
        const float rampAngle = glm::radians(20.0f);
        ramp.transform.position = glm::vec3(-6.0f, 1.0f, 0.0f);
        ramp.transform.setRotation(glm::quat(glm::vec3(0.0f, 0.0f, rampAngle)));
        const CollisionShape rampShape(BoxShape(glm::vec3(2.0f, 0.2f, 3.0f)));
        world.createStaticBody(rampShape, ramp.transform.position, ramp.transform.rotation(), terrain);

        // 60度陡坡：超过角色的可站立角度，角色会沿其下滑而不是站住。
        CubeOptions cliffOptions;
        cliffOptions.name = "cliff";
        cliffOptions.color = glm::vec4(0.55f, 0.4f, 0.38f, 1.0f);
        cliffOptions.size = glm::vec3(3.0f, 4.0f, 4.0f);
        GameObject &cliff = scene.createCube(cliffOptions);
        const float cliffAngle = glm::radians(60.0f);
        cliff.transform.position = glm::vec3(6.0f, 2.0f, -4.0f);
        cliff.transform.setRotation(glm::quat(glm::vec3(0.0f, 0.0f, cliffAngle)));
        world.createStaticBody(CollisionShape(BoxShape(glm::vec3(1.5f, 2.0f, 2.0f))), cliff.transform.position,
            cliff.transform.rotation(), terrain);

        // 台阶：不是自动攀爬，撞上去会被挡住（首版没有台阶踏升）。
        for (int step = 0; step < 3; ++step)
        {
            CubeOptions stepOptions;
            stepOptions.name = "step";
            stepOptions.color = glm::vec4(0.52f, 0.52f, 0.55f, 1.0f);
            stepOptions.size = glm::vec3(2.0f, 0.4f, 1.0f);
            GameObject &stepObject = scene.createCube(stepOptions);
            const glm::vec3 center(0.0f, 0.2f + static_cast<float>(step) * 0.4f,
                6.0f - static_cast<float>(step) * 1.0f);
            stepObject.transform.position = center;
            world.createStaticBody(CollisionShape(BoxShape(glm::vec3(1.0f, 0.2f, 0.5f))), center, NO_ROTATION,
                terrain);
        }

        // 两个可被射线命中的方块，放在Prop层便于演示层过滤。
        PhysicsFilter prop;
        prop.layer = PhysicsLayers::Prop;
        for (int index = 0; index < 2; ++index)
        {
            CubeOptions boxOptions;
            boxOptions.name = "prop";
            boxOptions.color = glm::vec4(0.75f, 0.62f, 0.32f, 1.0f);
            boxOptions.size = glm::vec3(1.0f);
            GameObject &propObject = scene.createCube(boxOptions);
            const glm::vec3 center(-3.0f + static_cast<float>(index) * 6.0f, 0.5f, -6.0f);
            propObject.transform.position = center;
            // 用组件方式登记：物体被删除时静态体随之注销，层由过滤参数指定。
            propObject.setPhysicsBody(world, CollisionShape(BoxShape(glm::vec3(0.5f))), prop);
        }
    }

    void spawnBall(Scene &scene, DemoState &state)
    {
        const float offset = static_cast<float>(state.spawnCounter % 5) * 0.35f - 0.7f;
        ++state.spawnCounter;
        const glm::vec3 position = state.character.position +
            glm::vec3(glm::cos(state.cameraYaw) * 1.6f + offset, 2.5f, glm::sin(state.cameraYaw) * 1.6f);

        SphereOptions options;
        options.name = "ball";
        options.color = glm::vec4(0.85f, 0.35f, 0.3f, 1.0f);
        options.radius = BALL_RADIUS;
        GameObject &ball = scene.createSphere(options);
        ball.transform.position = position;

        RigidBodySettings settings;
        settings.restitution = 0.45f;
        settings.friction = 0.4f;
        const PhysicsBodyId body = state.world->createDynamicBody(CollisionShape(SphereShape(BALL_RADIUS)),
            position, settings, state.propFilter);
        // 给一点自转与初速度，让示例看起来不像"垂直落下的小球"。
        state.world->setBodyAngularVelocity(body, glm::vec3(1.5f, 0.0f, -2.0f));
        state.world->setBodyVelocity(body, glm::vec3(offset * 2.0f, 0.0f, 0.0f));
        state.balls.push_back(BallVisual{body, ball.id()});
    }

    void clearDynamic(Scene &scene, DemoState &state)
    {
        for (const BallVisual &ball : state.balls)
        {
            scene.removeObject(ball.object);
        }
        state.balls.clear();
        for (const ObjectId object : state.placedBoxes)
        {
            // 放置的方块通过PhysicsBodyComponent登记，删除物体即注销静态体。
            scene.removeObject(object);
        }
        state.placedBoxes.clear();
    }

    // 射线检测演示：从摄像机沿视线发射，在命中点前侧放一个静态方块。
    bool placeBoxAtLook(Scene &scene, DemoState &state)
    {
        const glm::vec3 forward = glm::normalize(glm::vec3(std::cos(state.cameraYaw) * std::cos(state.cameraPitch),
            std::sin(state.cameraPitch),
            std::sin(state.cameraYaw) * std::cos(state.cameraPitch)));
        const glm::vec3 origin = state.character.position + glm::vec3(0.0f, 1.0f, 0.0f) + forward * 0.5f;
        const Ray ray(origin, forward);
        const auto hit = state.world->raycast(ray, 60.0f);
        if (!hit)
        {
            LOG_INFO("raycast missed every collider");
            return false;
        }
        LOG_INFO("raycast hit distance " + std::to_string(hit->hit.t));

        const glm::vec3 position = hit->hit.point + hit->hit.normal * (PLACED_BOX_HALF + 0.01f);
        CubeOptions options;
        options.name = "placed";
        options.color = glm::vec4(0.4f, 0.72f, 0.85f, 1.0f);
        options.size = glm::vec3(PLACED_BOX_HALF * 2.0f);
        GameObject &placed = scene.createCube(options);
        placed.transform.position = position;
        // 演示GameObject的物理组件：位置取物体当前Transform，删除物体时自动注销。
        placed.setPhysicsBody(*state.world, CollisionShape(BoxShape(glm::vec3(PLACED_BOX_HALF))),
            state.propFilter);
        state.placedBoxes.push_back(placed.id());
        return true;
    }

    void rebuildDebugVisuals(Scene &scene, DemoState &state)
    {
        for (const DebugVisual &visual : state.debugVisuals)
        {
            scene.removeObject(visual.object);
        }
        state.debugVisuals.clear();
        state.debugMaterials.clear(); // 物体已删除，可以安全释放上一轮的叠加材质。

        for (const PhysicsBodyId body : state.world->bodyIds())
        {
            const CollisionShape *shape = state.world->bodyShape(body);
            const auto position = state.world->bodyPosition(body);
            const auto rotation = state.world->bodyRotation(body);
            if (shape == nullptr || !position || !rotation)
            {
                continue;
            }
            // 动态体用另一套颜色，便于把"会被求解的物体"和静态地形区分开。
            const bool dynamic = state.world->isDynamicBody(body);
            const glm::vec4 color = dynamic ? glm::vec4(0.95f, 0.55f, 0.25f, 0.40f)
                                            : glm::vec4(0.35f, 0.75f, 0.95f, 0.30f);

            GameObject *overlay = nullptr;
            bool fixedPose = false;
            if (shape->holds<SphereShape>())
            {
                SphereOptions options;
                options.name = "debug";
                options.color = glm::vec4(1.0f);
                options.radius = shape->get<SphereShape>().radius;
                overlay = &scene.createSphere(options);
            }
            else if (shape->holds<BoxShape>())
            {
                CubeOptions options;
                options.name = "debug";
                options.color = glm::vec4(1.0f);
                options.size = shape->get<BoxShape>().halfExtents * 2.0f;
                overlay = &scene.createCube(options);
            }
            else if (shape->holds<CapsuleShape>())
            {
                // 首版没有胶囊网格：用等高的圆柱近似表示胶囊的包围体。
                const CapsuleShape &capsule = shape->get<CapsuleShape>();
                CylinderOptions options;
                options.name = "debug";
                options.color = glm::vec4(1.0f);
                options.radius = capsule.radius;
                options.height = capsule.cylinderHeight + capsule.radius * 2.0f;
                overlay = &scene.createCylinder(options);
            }
            else if (shape->holds<PlaneShape>())
            {
                PlaneOptions options;
                options.name = "debug";
                options.color = glm::vec4(1.0f);
                options.size = glm::vec2(40.0f);
                overlay = &scene.createPlane(options);
                fixedPose = true; // 无限平面始终在世界原点，只抬高一点避免z-fighting。
            }
            if (overlay == nullptr)
            {
                continue;
            }

            // 把不透明的内置材质替换成示例持有的半透明副本：叠加显示但不遮挡原有几何。
            const std::shared_ptr<Material> material = makeTranslucentMaterial(overlay->renderable(), color);
            overlay->setRenderable(*overlay->renderable().mesh(), *material);
            state.debugMaterials.push_back(material);

            if (fixedPose)
            {
                overlay->transform.position = glm::vec3(0.0f, 0.008f, 0.0f);
            }
            else
            {
                overlay->transform.position = *position;
                overlay->transform.setRotation(*rotation);
            }
            state.debugVisuals.push_back(DebugVisual{body, overlay->id(), fixedPose});
        }
        LOG_INFO("debug collider overlay rebuilt: " + std::to_string(state.debugVisuals.size()) + " shapes");
    }

    void updateCharacter(Application &application, DemoState &state, float deltaTime)
    {
        if (deltaTime <= 0.0f)
        {
            return;
        }
        const Input &input = application.input();
        const Camera &camera = application.camera();

        // 摄像机相对移动：W/S沿视线，D/A沿屏幕右方。
        // 方向约定集中在cameraRelativeDirection里并单独测试，避免再次出现左右颠倒。
        const glm::vec3 toTarget = state.character.position + glm::vec3(0.0f, 0.6f, 0.0f) - camera.position();

        const float forwardInput = float(input.isKeyDown(Key::W)) - float(input.isKeyDown(Key::S));
        const float rightInput = float(input.isKeyDown(Key::D)) - float(input.isKeyDown(Key::A));
        glm::vec3 direction = cameraRelativeDirection(toTarget, forwardInput, rightInput);
        const float speed = (input.isKeyDown(Key::LeftShift) || input.isKeyDown(Key::RightShift))
            ? CHARACTER_SPRINT
            : CHARACTER_SPEED;
        state.character.velocity.x = 0.0f;
        state.character.velocity.z = 0.0f;
        if (glm::length(direction) > 0.0f)
        {
            direction = glm::normalize(direction);
            state.character.velocity.x = direction.x * speed;
            state.character.velocity.z = direction.z * speed;
            state.characterYaw = std::atan2(direction.x, direction.z);
        }

        // 重力与跳跃由示例决定，控制器只负责碰撞与着地判定。
        state.character.velocity.y += state.world->gravity().y * deltaTime;
        if (state.character.grounded && state.character.velocity.y <= 0.0f)
        {
            state.character.velocity.y = -1.0f; // 轻微下压，保证下坡时贴地而不是漂浮。
        }
        if (input.wasKeyPressed(Key::Space) && state.character.grounded)
        {
            state.character.velocity.y = CHARACTER_JUMP_SPEED;
        }

        state.character = state.controller.move(*state.world, state.character, deltaTime, state.characterMask);
    }

    void updateCamera(Application &application, DemoState &state, float deltaTime)
    {
        const Input &input = application.input();
        if (input.isMouseButtonDown(MouseButton::Right))
        {
            if (state.looking)
            {
                const glm::dvec2 delta = input.mouseDelta();
                state.cameraYaw = static_cast<float>(std::remainder(
                    double(state.cameraYaw) + delta.x * MOUSE_SENSITIVITY, glm::two_pi<double>()));
                state.cameraPitch = std::clamp(state.cameraPitch -
                    static_cast<float>(delta.y * MOUSE_SENSITIVITY), -0.4f, 1.2f);
            }
            state.looking = true;
        }
        else
        {
            state.looking = false;
        }
        (void)deltaTime;

        const glm::vec3 target = state.character.position + glm::vec3(0.0f, 0.6f, 0.0f);
        const float horizontal = std::cos(state.cameraPitch);
        const glm::vec3 offset(std::cos(state.cameraYaw) * horizontal, std::sin(state.cameraPitch),
            std::sin(state.cameraYaw) * horizontal);
        application.camera().setView(target + offset * CAMERA_DISTANCE, target, UP);
    }

    void syncVisuals(Scene &scene, DemoState &state)
    {
        // 角色：圆柱可视物体直接跟随控制器给出的胶囊中心。
        if (GameObject *character = scene.findObject(state.characterVisual))
        {
            character->transform.position = state.character.position;
            character->transform.setRotation(glm::quat(glm::vec3(0.0f, state.characterYaw, 0.0f)));
        }

        // 动态球：用插值状态渲染，消除固定步长与渲染帧率不一致时的台阶感。
        const float alpha = state.world->interpolationAlpha();
        for (BallVisual &ball : state.balls)
        {
            GameObject *object = scene.findObject(ball.object);
            const auto interpolated = state.world->interpolatedBodyState(ball.body, alpha);
            if (object == nullptr || !interpolated)
            {
                continue;
            }
            object->transform.position = interpolated->position;
            object->transform.setRotation(interpolated->rotation);
        }

        // 调试叠加：动态体的形状会移动，每帧同步位姿；平面保持固定高度。
        const PhysicsWorld &world = *state.world;
        for (const DebugVisual &visual : state.debugVisuals)
        {
            if (visual.fixedPose)
            {
                continue;
            }
            GameObject *overlay = scene.findObject(visual.object);
            const auto position = world.bodyPosition(visual.body);
            const auto rotation = world.bodyRotation(visual.body);
            if (overlay == nullptr || !position || !rotation)
            {
                continue;
            }
            overlay->transform.position = *position;
            overlay->transform.setRotation(*rotation);
        }
    }
}

int main(int argc, char *argv[])
{
    // PhysicsWorld必须在Application之前创建、之后销毁：Scene里的物体借用了它。
    auto world = std::make_shared<PhysicsWorld>();
    auto state = std::make_shared<DemoState>();
    state->world = world;
    state->character.position = glm::vec3(0.0f, 2.5f, 0.0f);

    return runExample(argc, argv, "physics_demo", "physics_demo | WASD 移动 Space 跳 右键拖拽转视角 E 生成球 R 射线放盒子 P 调试碰撞体 C 清空",
        glm::vec4(0.06f, 0.07f, 0.10f, 1.0f),
        [state](Application &application, const std::filesystem::path &)
        {
            Scene &scene = application.scene();
            scene.lighting().mainLight().direction = glm::vec3(-0.4f, -1.0f, -0.35f);
            scene.lighting().mainLight().intensity = 1.15f;
            scene.lighting().ambient() = glm::vec3(0.28f);
            createTerrain(scene, *state->world);

            // 角色可视物体：首版没有胶囊网格，用等高的圆柱近似，位置与胶囊中心一致。
            CylinderOptions characterOptions;
            characterOptions.name = "character";
            characterOptions.color = glm::vec4(0.35f, 0.65f, 0.95f, 1.0f);
            characterOptions.radius = state->controller.settings().radius;
            characterOptions.height = state->controller.settings().cylinderHeight +
                state->controller.settings().radius * 2.0f;
            GameObject &character = scene.createCylinder(characterOptions);
            character.transform.position = state->character.position;
            state->characterVisual = character.id();

            LOG_INFO("physics_demo ready: WASD move, Space jump, E spawn ball, R place box, P debug shapes");
        },
        ExampleFrameContent::DrawnScene,
        [state](Application &application, float deltaTime)
        {
            Scene &scene = application.scene();
            const Input &input = application.input();

            updateCharacter(application, *state, deltaTime);
            updateCamera(application, *state, deltaTime);

            if (input.wasKeyPressed(Key::E))
            {
                spawnBall(scene, *state);
            }
            if (input.wasKeyPressed(Key::R))
            {
                placeBoxAtLook(scene, *state);
            }
            if (input.wasKeyPressed(Key::P))
            {
                state->debugVisible = !state->debugVisible;
                if (state->debugVisible)
                {
                    rebuildDebugVisuals(scene, *state);
                }
                else
                {
                    for (const DebugVisual &visual : state->debugVisuals)
                    {
                        scene.removeObject(visual.object);
                    }
                    state->debugVisuals.clear();
                    // 物体已经删除，此时释放叠加材质不会留下悬空引用。
                    state->debugMaterials.clear();
                }
            }
            if (input.wasKeyPressed(Key::C))
            {
                // 清空动态球与放置的方块；调试叠加按新的物体集合重建。
                clearDynamic(scene, *state);
                if (state->debugVisible)
                {
                    rebuildDebugVisuals(scene, *state);
                }
            }
            if (input.wasKeyPressed(Key::G))
            {
                // 演示冲量接口：把镜头正前方的球推开，同时唤醒休眠中的球。
                for (const BallVisual &ball : state->balls)
                {
                    state->world->applyImpulse(ball.body, glm::vec3(0.0f, 2.5f, 0.0f));
                }
            }

            // 固定步长推进：world内部按1/60累加，最多补跑4步。
            state->world->step(deltaTime);
            syncVisuals(scene, *state);
        });
}
