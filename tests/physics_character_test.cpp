#include "TestSupport.h"
#include "physics/character/CharacterController.h"
#include "physics/world/PhysicsWorld.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    const glm::quat NO_ROTATION(1.0f, 0.0f, 0.0f, 0.0f);
    const float STEP = 1.0f / 60.0f;

    void requireNear(float actual, float expected, float tolerance, const char *message)
    {
        require(std::abs(actual - expected) < tolerance, message);
    }

    // 反复推进直到着地，返回最后一步的状态。
    CharacterState settle(const CharacterController &controller, const PhysicsWorld &world, CharacterState state,
        int steps = 240)
    {
        for (int index = 0; index < steps; ++index)
        {
            state = controller.move(world, state, STEP);
            if (state.grounded)
            {
                break;
            }
        }
        return state;
    }
}

int main()
{
    try
    {
        // 参数校验：非法尺寸与角度必须拒绝。
        expectThrow<std::invalid_argument>([] { CharacterController(CharacterSettings{0.0f, 1.0f}); },
            "Zero character radius was accepted");
        expectThrow<std::invalid_argument>([] { CharacterController(CharacterSettings{0.4f, -1.0f}); },
            "Negative cylinder height was accepted");
        // 注意：CharacterController(badSlope); 会被解析为变量声明而不是临时构造，
        // 必须显式命名局部对象，否则校验代码根本不会执行。
        CharacterSettings badSlope;
        badSlope.maximumSlopeAngleDegrees = 90.0f;
        expectThrow<std::invalid_argument>([&] { CharacterController controller(badSlope); },
            "90 degree slope limit was accepted");
        CharacterSettings badSkin;
        badSkin.skinWidth = 0.0f;
        expectThrow<std::invalid_argument>([&] { CharacterController controller(badSkin); },
            "Zero skin width was accepted");
        CharacterSettings badIterations;
        badIterations.maximumSlideIterations = 0;
        expectThrow<std::invalid_argument>([&] { CharacterController controller(badIterations); },
            "Zero slide iterations were accepted");

        CharacterSettings settings;
        settings.radius = 0.5f;
        settings.cylinderHeight = 0.0f; // 退化成球体，便于用解析值核对结果。
        const CharacterController controller(settings);
        require(controller.shape().holds<CapsuleShape>(), "Character shape is not a capsule");
        requireNear(controller.capCenterOffset(), 0.0f, 0.0001f, "Zero-height capsule offset is wrong");

        // 落到地面平面上：着地高度等于半径（允许皮肤宽度的量级误差）。
        PhysicsWorld groundWorld;
        groundWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)),
            glm::vec3(0.0f));
        CharacterState falling;
        falling.position = glm::vec3(0.0f, 3.0f, 0.0f);
        falling.velocity = glm::vec3(0.0f, -5.0f, 0.0f);
        const CharacterState landed = settle(controller, groundWorld, falling);
        require(landed.grounded, "Character never landed on the ground plane");
        requireNear(landed.position.y, settings.radius, 0.03f, "Landed height is wrong");
        requireNear(landed.velocity.y, 0.0f, 0.0001f, "Downward velocity was not cleared on landing");
        requireNear(landed.groundNormal.y, 1.0f, 0.0001f, "Ground normal is wrong");

        // 零时长更新只做着地检测，不移动。
        const CharacterState holdStill = controller.move(groundWorld, landed, 0.0f);
        requireNear(holdStill.position.x, landed.position.x, 0.0001f, "Zero dt moved the character");
        requireNear(holdStill.position.y, landed.position.y, 0.0001f, "Zero dt changed the height");
        require(holdStill.grounded, "Zero dt lost the grounded state");

        // 空世界自由落体：没有碰撞时位移严格等于v*dt。
        PhysicsWorld emptyWorld;
        CharacterState freeFall;
        freeFall.position = glm::vec3(0.0f, 10.0f, 0.0f);
        freeFall.velocity = glm::vec3(2.0f, -1.0f, 0.0f);
        const CharacterState fallen = controller.move(emptyWorld, freeFall, STEP);
        require(!fallen.grounded, "Character reported ground in an empty world");
        requireNear(fallen.position.x, 2.0f * STEP, 0.0001f, "Free fall horizontal motion is wrong");
        requireNear(fallen.position.y, 10.0f - STEP, 0.0001f, "Free fall vertical motion is wrong");

        // 撞墙：水平位移被墙面吸收，沿墙的切向位移保留。
        PhysicsWorld wallWorld;
        wallWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)), glm::vec3(0.0f));
        BoxShape wallShape(glm::vec3(0.5f, 2.0f, 5.0f));
        const CollisionShape wall(wallShape);
        wallWorld.createStaticBody(wall, glm::vec3(2.0f, 2.0f, 0.0f));
        CharacterState walking;
        walking.position = glm::vec3(0.0f, 0.5f, 0.0f);
        walking.velocity = glm::vec3(4.0f, 0.0f, 3.0f);
        for (int index = 0; index < 60; ++index)
        {
            walking = controller.move(wallWorld, walking, STEP);
        }
        require(walking.position.x < 2.0f - 0.5f - settings.radius + 0.05f,
            "Character walked into the wall");        require(walking.position.z > 0.5f, "Character did not slide along the wall");
        require(walking.velocity.x <= 0.001f, "Velocity into the wall was not removed");
        require(walking.velocity.z > 0.0f, "Tangential velocity was removed by the wall");

        // 台阶：首版没有踏升逻辑。高出台阶会被挡住；低于角色半径的矮台阶会被圆角底面翻上去
        // （球底/胶囊底对盒体的穿透修正本来就会产生向上的分量，这里显式记录该行为）。
        PhysicsWorld blockWorld;
        blockWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)), glm::vec3(0.0f));
        const CollisionShape tallBlock(BoxShape(glm::vec3(1.0f, 0.4f, 1.0f))); // 高0.8，高于半径0.5
        blockWorld.createStaticBody(tallBlock, glm::vec3(2.0f, 0.4f, 0.0f));
        CharacterState blocked;
        blocked.position = glm::vec3(0.0f, 0.5f, 0.0f);
        blocked.velocity = glm::vec3(3.0f, 0.0f, 0.0f);
        for (int index = 0; index < 90; ++index)
        {
            blocked = controller.move(blockWorld, blocked, STEP);
        }
        require(blocked.position.x < 1.0f, "Character climbed a block taller than its radius");
        requireNear(blocked.position.y, 0.5f, 0.02f, "Blocked character changed height");

        PhysicsWorld stepWorld;
        stepWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)), glm::vec3(0.0f));
        const CollisionShape lowStep(BoxShape(glm::vec3(1.0f, 0.1f, 1.0f))); // 高0.2，低于半径
        stepWorld.createStaticBody(lowStep, glm::vec3(2.0f, 0.1f, 0.0f));
        CharacterState stepping;
        stepping.position = glm::vec3(0.0f, 0.5f, 0.0f);
        stepping.velocity = glm::vec3(3.0f, 0.0f, 0.0f);
        for (int index = 0; index < 90; ++index)
        {
            stepping = controller.move(stepWorld, stepping, STEP);
        }
        require(stepping.position.y > 0.6f, "Sphere bottom did not ride over the low step");

        // 斜坡：缓坡可站立，陡坡不可站立。
        PhysicsWorld slopeWorld;
        const float slopeAngle = glm::radians(20.0f);
        const glm::vec3 slopeNormal = glm::normalize(glm::vec3(-std::sin(slopeAngle), std::cos(slopeAngle), 0.0f));
        slopeWorld.createStaticBody(CollisionShape(PlaneShape(slopeNormal, 0.0f)), glm::vec3(0.0f));
        CharacterState onSlope;
        onSlope.position = glm::vec3(0.0f, 1.0f, 0.0f);
        onSlope.velocity = glm::vec3(0.0f, -2.0f, 0.0f);
        const CharacterState standing = settle(controller, slopeWorld, onSlope);
        require(standing.grounded, "Character did not stand on a 20 degree slope");

        PhysicsWorld cliffWorld;
        const float cliffAngle = glm::radians(70.0f);
        const glm::vec3 cliffNormal = glm::normalize(glm::vec3(-std::sin(cliffAngle), std::cos(cliffAngle), 0.0f));
        cliffWorld.createStaticBody(CollisionShape(PlaneShape(cliffNormal, 0.0f)), glm::vec3(0.0f));
        CharacterState onCliff;
        onCliff.position = glm::vec3(0.0f, 1.0f, 0.0f);
        onCliff.velocity = glm::vec3(0.0f, -2.0f, 0.0f);
        const CharacterState sliding = settle(controller, cliffWorld, onCliff, 60);
        require(!sliding.grounded, "Character stood on a 70 degree slope");
        require(sliding.position.y < 1.0f, "Character did not slide down the steep slope");

        // 跳跃：竖直速度为正时不吸附地面，位置随即上升。
        CharacterState jumping = landed;
        jumping.velocity.y = 5.0f;
        const CharacterState inAir = controller.move(groundWorld, jumping, STEP);
        require(!inAir.grounded, "Jumping character was still grounded");
        require(inAir.position.y > landed.position.y, "Jump did not raise the character");
        requireNear(inAir.velocity.y, 5.0f, 0.0001f, "Jump velocity was changed");

        // 层掩码：掩码排除地形层时，角色会穿过地面。
        const PhysicsBodyId groundBody = groundWorld.bodyIds().front();
        require(groundWorld.bodyLayer(groundBody) == PhysicsLayers::Default, "Default layer is wrong");
        const CharacterState throughGround = controller.move(groundWorld, falling, STEP,
            PhysicsLayers::Character);
        require(!throughGround.grounded, "Query mask did not filter out the ground");

        // 非法dt与非法状态。
        expectThrow<std::invalid_argument>([&] { controller.move(groundWorld, landed, -1.0f); },
            "Negative delta time was accepted");
        CharacterState invalid = landed;
        invalid.position = glm::vec3(std::numeric_limits<float>::quiet_NaN());
        expectThrow<std::invalid_argument>([&] { controller.move(groundWorld, invalid, STEP); },
            "NaN position was accepted");

        std::cout << "physics_character_test passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "physics_character_test failed: " << error.what() << '\n';
        return 1;
    }
}
