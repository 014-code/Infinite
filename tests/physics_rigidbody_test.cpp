#include "TestSupport.h"
#include "physics/world/PhysicsWorld.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    const float STEP = 1.0f / 60.0f;

    void requireNear(float actual, float expected, float tolerance, const char *message)
    {
        require(std::abs(actual - expected) < tolerance, message);
    }
}

int main()
{
    try
    {
        const CollisionShape sphere(SphereShape(0.5f));
        const CollisionShape box(BoxShape(glm::vec3(0.5f)));

        // 动态体只支持球体；非法参数必须在创建时拒绝。
        PhysicsWorld world;
        expectThrow<std::invalid_argument>([&] { world.createDynamicBody(box, glm::vec3(0.0f)); },
            "Box dynamic body was accepted");
        RigidBodySettings badMass;
        badMass.mass = 0.0f;
        expectThrow<std::invalid_argument>([&] { world.createDynamicBody(sphere, glm::vec3(0.0f), badMass); },
            "Zero mass was accepted");
        RigidBodySettings badRestitution;
        badRestitution.restitution = 1.5f;
        expectThrow<std::invalid_argument>([&] { world.createDynamicBody(sphere, glm::vec3(0.0f), badRestitution); },
            "Restitution above 1 was accepted");
        expectThrow<std::invalid_argument>([&] { world.setFixedStep(0.0f); }, "Zero fixed step was accepted");
        expectThrow<std::invalid_argument>([&] { world.setSolverIterations(0); }, "Zero solver iterations accepted");
        expectThrow<std::invalid_argument>([&] { world.step(-0.5f); }, "Negative frame delta was accepted");

        // 固定步回调：一帧补跑多个子步时，每次回调都收到相同的固定dt。
        {
            int callbackCount = 0;
            world.step(STEP * 2.0f, [&](float fixedDeltaTime)
            {
                ++callbackCount;
                requireNear(fixedDeltaTime, world.fixedStep(), 0.000001f,
                    "Fixed callback received a variable delta time");
            });
            require(callbackCount == 2, "Fixed callback count does not match simulated steps");
        }

        // 自由落体：无碰撞、无阻尼时速度与位移符合运动学。
        {
            PhysicsWorld fallWorld;
            fallWorld.setGravity(glm::vec3(0.0f, -10.0f, 0.0f));
            RigidBodySettings settings;
            settings.linearDamping = 0.0f;
            settings.allowSleep = false;
            const PhysicsBodyId body = fallWorld.createDynamicBody(sphere, glm::vec3(0.0f, 10.0f, 0.0f), settings);
            require(fallWorld.isDynamicBody(body), "Dynamic body was not marked dynamic");
            require(fallWorld.dynamicBodyCount() == 1, "Dynamic body count is wrong");
            for (int index = 0; index < 60; ++index)
            {
                fallWorld.step(STEP);
            }
            const auto state = fallWorld.bodyState(body);
            require(state.has_value(), "Dynamic body has no state");
            requireNear(state->velocity.y, -10.0f, 0.2f, "Free fall velocity is wrong");
            requireNear(state->position.y, 5.0f, 0.2f, "Free fall position is wrong");
            require(fallWorld.fixedStepCount() == 60, "Fixed step count is wrong");
        }

        // 落到地面后静止并进入休眠，位置稳定不漂移。
        {
            PhysicsWorld restWorld;
            restWorld.setGravity(glm::vec3(0.0f, -10.0f, 0.0f));
            restWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)), glm::vec3(0.0f));
            RigidBodySettings settings;
            settings.restitution = 0.0f;
            settings.sleepTime = 0.2f;
            const PhysicsBodyId body = restWorld.createDynamicBody(sphere, glm::vec3(0.0f, 0.6f, 0.0f), settings);
            for (int index = 0; index < 240; ++index)
            {
                restWorld.step(STEP);
            }
            const auto state = restWorld.bodyState(body);
            require(state.has_value(), "Resting body has no state");
            require(state->sleeping, "Resting body did not fall asleep");
            requireNear(state->position.y, 0.5f, 0.02f, "Resting height is wrong");
            const float restingHeight = state->position.y;
            for (int index = 0; index < 120; ++index)
            {
                restWorld.step(STEP);
            }
            requireNear(restWorld.bodyState(body)->position.y, restingHeight, 0.0001f,
                "Sleeping body drifted");
            require(restWorld.isSleeping(body), "Sleeping flag was lost");
        }

        // 弹性碰撞：恢复系数为1时球体反弹，速度方向翻转且大小接近。
        {
            PhysicsWorld bounceWorld;
            bounceWorld.setGravity(glm::vec3(0.0f, -10.0f, 0.0f));
            bounceWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)), glm::vec3(0.0f));
            RigidBodySettings settings;
            settings.restitution = 1.0f;
            settings.linearDamping = 0.0f;
            settings.allowSleep = false;
            const PhysicsBodyId body = bounceWorld.createDynamicBody(sphere, glm::vec3(0.0f, 2.0f, 0.0f), settings);
            bounceWorld.setBodyVelocity(body, glm::vec3(0.0f, -4.0f, 0.0f));
            bool bounced = false;
            for (int index = 0; index < 60 && !bounced; ++index)
            {
                bounceWorld.step(STEP);
                bounced = bounceWorld.bodyState(body)->velocity.y > 1.0f;
            }
            require(bounced, "Elastic sphere did not bounce");
            // 单个固定步内球体最多下移v*h，位置修正保留少量穿透（约一步位移的一小部分）。
            require(bounceWorld.bodyState(body)->position.y >= 0.5f - 0.05f,
                "Bouncing sphere sank into the ground");
        }

        // 非弹性碰撞：恢复系数为0时竖直速度被吸收，不会持续弹跳。
        {
            PhysicsWorld softWorld;
            softWorld.setGravity(glm::vec3(0.0f, -10.0f, 0.0f));
            softWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)), glm::vec3(0.0f));
            RigidBodySettings settings;
            settings.restitution = 0.0f;
            settings.allowSleep = false;
            const PhysicsBodyId body = softWorld.createDynamicBody(sphere, glm::vec3(0.0f, 2.0f, 0.0f), settings);
            for (int index = 0; index < 120; ++index)
            {
                softWorld.step(STEP);
            }
            const auto state = softWorld.bodyState(body);
            require(state->position.y < 0.56f, "Inelastic sphere bounced too high");
            require(std::abs(state->velocity.y) < 0.5f, "Inelastic sphere kept vertical speed");
        }

        // 球体互撞：等质量正碰后速度大致交换。
        {
            PhysicsWorld pairWorld;
            pairWorld.setGravity(glm::vec3(0.0f));
            RigidBodySettings settings;
            settings.restitution = 1.0f;
            settings.linearDamping = 0.0f;
            settings.allowSleep = false;
            const PhysicsBodyId left = pairWorld.createDynamicBody(sphere, glm::vec3(-2.0f, 0.0f, 0.0f), settings);
            const PhysicsBodyId right = pairWorld.createDynamicBody(sphere, glm::vec3(2.0f, 0.0f, 0.0f), settings);
            pairWorld.setBodyVelocity(left, glm::vec3(3.0f, 0.0f, 0.0f));
            pairWorld.setBodyVelocity(right, glm::vec3(-3.0f, 0.0f, 0.0f));
            for (int index = 0; index < 240; ++index)
            {
                pairWorld.step(STEP);
            }
            const auto leftState = pairWorld.bodyState(left);
            const auto rightState = pairWorld.bodyState(right);
            require(leftState->position.x < rightState->position.x, "Spheres passed through each other");
            require(leftState->velocity.x < 0.0f && rightState->velocity.x > 0.0f,
                "Sphere velocities were not exchanged");
            require(glm::length(leftState->position - rightState->position) >= 1.0f - 0.05f,
                "Spheres did not separate");
        }

        // 重叠的球体被位置修正推开，不会永久卡在一起。
        {
            PhysicsWorld overlapWorld;
            overlapWorld.setGravity(glm::vec3(0.0f));
            RigidBodySettings settings;
            settings.restitution = 0.0f;
            settings.allowSleep = false;
            const PhysicsBodyId a = overlapWorld.createDynamicBody(sphere, glm::vec3(0.0f, 0.0f, 0.0f), settings);
            const PhysicsBodyId b = overlapWorld.createDynamicBody(sphere, glm::vec3(0.4f, 0.0f, 0.0f), settings);
            for (int index = 0; index < 60; ++index)
            {
                overlapWorld.step(STEP);
            }
            const float distance = glm::length(*overlapWorld.bodyPosition(a) - *overlapWorld.bodyPosition(b));
            require(distance > 0.95f, "Overlapping spheres were not pushed apart");
        }

        // 冲量唤醒休眠体并改变速度。
        {
            PhysicsWorld sleepWorld;
            sleepWorld.setGravity(glm::vec3(0.0f, -10.0f, 0.0f));
            sleepWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)), glm::vec3(0.0f));
            const PhysicsBodyId body = sleepWorld.createDynamicBody(sphere, glm::vec3(0.0f, 0.5f, 0.0f));
            for (int index = 0; index < 120; ++index)
            {
                sleepWorld.step(STEP);
            }
            require(sleepWorld.isSleeping(body), "Free body did not fall asleep");
            require(sleepWorld.applyImpulse(body, glm::vec3(0.0f, 2.0f, 0.0f)), "Applying an impulse failed");
            require(!sleepWorld.isSleeping(body), "Impulse did not wake the body");
            const auto awake = sleepWorld.bodyState(body);
            requireNear(awake->velocity.y, 2.0f, 0.0001f, "Impulse did not change velocity");
            require(!sleepWorld.applyImpulse(999, glm::vec3(1.0f)), "Invalid impulse target reported success");
            require(sleepWorld.wakeBody(body), "Explicit wake failed");
            expectThrow<std::invalid_argument>([&] {
                sleepWorld.setBodyVelocity(body, glm::vec3(std::numeric_limits<float>::quiet_NaN())); },
                "NaN velocity was accepted");
        }

        // 力在固定步中按F=ma积分，并在该步结束后清零；第二步不应重复施加。
        {
            PhysicsWorld forceWorld;
            forceWorld.setGravity(glm::vec3(0.0f));
            RigidBodySettings settings;
            settings.linearDamping = 0.0f;
            settings.allowSleep = false;
            const PhysicsBodyId body = forceWorld.createDynamicBody(sphere, glm::vec3(0.0f), settings);
            require(forceWorld.applyForce(body, glm::vec3(6.0f, 0.0f, 0.0f)),
                "Applying a force failed");
            forceWorld.step(STEP);
            const float firstVelocity = forceWorld.bodyState(body)->velocity.x;
            forceWorld.step(STEP);
            const float secondVelocity = forceWorld.bodyState(body)->velocity.x;
            requireNear(firstVelocity, 6.0f * STEP, 0.0001f, "Force integration is wrong");
            requireNear(secondVelocity, firstVelocity, 0.0001f, "Force was not cleared after a step");
            require(!forceWorld.applyForce(999, glm::vec3(1.0f)),
                "Invalid force target reported success");
        }

        // 渲染插值：系数0取上一步位姿，1取当前位姿，中间值位于两者之间。
        {
            PhysicsWorld interpolateWorld;
            interpolateWorld.setGravity(glm::vec3(0.0f, -10.0f, 0.0f));
            RigidBodySettings settings;
            settings.allowSleep = false;
            const PhysicsBodyId body = interpolateWorld.createDynamicBody(sphere, glm::vec3(0.0f, 5.0f, 0.0f), settings);
            interpolateWorld.step(STEP);
            const auto current = interpolateWorld.bodyState(body);
            const auto previous = interpolateWorld.interpolatedBodyState(body, 0.0f);
            const auto middle = interpolateWorld.interpolatedBodyState(body, 0.5f);
            require(current.has_value() && previous.has_value() && middle.has_value(),
                "Interpolated state is missing");
            require(previous->position.y > current->position.y, "Previous pose is not older than current");
            require(middle->position.y < previous->position.y && middle->position.y > current->position.y,
                "Interpolated pose is not between the two steps");
            requireNear(interpolateWorld.interpolationAlpha(), 0.0f, 0.0001f,
                "Alpha after a full step should be near zero");
            expectThrow<std::invalid_argument>([&] { interpolateWorld.interpolatedBodyState(body,
                std::numeric_limits<float>::quiet_NaN()); }, "NaN interpolation alpha was accepted");
        }

        // 碰撞层过滤：掩码不接受地形层时，球体会穿过无限地面。
        {
            PhysicsWorld layerWorld;
            layerWorld.setGravity(glm::vec3(0.0f, -10.0f, 0.0f));
            PhysicsFilter terrain;
            terrain.layer = PhysicsLayers::Terrain;
            terrain.mask = 0xFFFFFFFFu;
            layerWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)), glm::vec3(0.0f),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f), terrain);
            PhysicsFilter prop;
            prop.layer = PhysicsLayers::Prop;
            prop.mask = PhysicsLayers::Prop; // 不接受地形层。
            RigidBodySettings settings;
            settings.allowSleep = false;
            const PhysicsBodyId body = layerWorld.createDynamicBody(sphere, glm::vec3(0.0f, 2.0f, 0.0f), settings, prop);
            require(layerWorld.bodyLayer(body) == PhysicsLayers::Prop, "Body layer was not stored");
            for (int index = 0; index < 120; ++index)
            {
                layerWorld.step(STEP);
            }
            require(layerWorld.bodyState(body)->position.y < 0.0f, "Layer filter did not let the body pass");
            require(layerWorld.broadphasePairs().empty(), "Filtered pair appeared in broadphase");

            // 恢复掩码后立刻产生接触并被挡住。
            PhysicsFilter contactProp = prop;
            contactProp.mask = 0xFFFFFFFFu;
            const PhysicsBodyId blocking = layerWorld.createDynamicBody(sphere, glm::vec3(3.0f, 2.0f, 0.0f), settings,
                contactProp);
            for (int index = 0; index < 240; ++index)
            {
                layerWorld.step(STEP);
            }
            require(layerWorld.bodyState(blocking)->position.y > 0.45f, "Unfiltered body fell through the ground");
        }

        // 宽相位接入step：分离的刚体不进入窄相位，避免对所有body做O(n²)全配对。
        {
            PhysicsWorld sweepWorld;
            sweepWorld.setGravity(glm::vec3(0.0f, -10.0f, 0.0f));
            sweepWorld.createStaticBody(CollisionShape(PlaneShape(glm::vec3(0.0f, 1.0f, 0.0f), 0.0f)), glm::vec3(0.0f));
            RigidBodySettings settings;
            settings.allowSleep = false;
            constexpr int ballCount = 50;
            for (int index = 0; index < ballCount; ++index)
            {
                sweepWorld.createDynamicBody(sphere,
                    glm::vec3(static_cast<float>(index) * 4.0f, 2.0f, 0.0f), settings);
            }
            sweepWorld.step(STEP);
            // 互不相交的球之间没有候选对，每个球只与无限地面平面配对；
            // 旧的全配对实现会产生约50×49/2 + 50 次窄相位调用。
            require(sweepWorld.lastNarrowphasePairCount() == static_cast<std::size_t>(ballCount),
                "Sweep broadphase did not reduce narrowphase pairs");
        }
        {
            // 互相重叠的两个球必须产生一对动态-动态候选，剪枝不能把它们漏掉。
            PhysicsWorld overlapWorld;
            overlapWorld.setGravity(glm::vec3(0.0f));
            RigidBodySettings settings;
            settings.allowSleep = false;
            overlapWorld.createDynamicBody(sphere, glm::vec3(0.0f), settings);
            overlapWorld.createDynamicBody(sphere, glm::vec3(0.8f, 0.0f, 0.0f), settings);
            overlapWorld.step(STEP);
            require(overlapWorld.lastNarrowphasePairCount() == 1,
                "Overlapping bodies did not produce a narrowphase pair");
        }
        {
            // 高速移动的刚体必须使用积分后的AABB参与宽相位，否则会沿用上一帧的包围盒而漏检。
            PhysicsWorld fastWorld;
            fastWorld.setGravity(glm::vec3(0.0f));
            fastWorld.createStaticBody(CollisionShape(BoxShape(glm::vec3(0.5f))), glm::vec3(0.0f));
            RigidBodySettings settings;
            settings.linearDamping = 0.0f;
            settings.allowSleep = false;
            const PhysicsBodyId body = fastWorld.createDynamicBody(sphere, glm::vec3(-3.0f, 0.0f, 0.0f),
                settings);
            fastWorld.setBodyVelocity(body, glm::vec3(180.0f, 0.0f, 0.0f));
            fastWorld.step(STEP);
            require(fastWorld.lastNarrowphasePairCount() == 1,
                "Fast-moving sphere was missed by the broadphase");
        }
        {
            // 双方都休眠后不再需要接触：统计值应回落到0（无平面场景）。
            PhysicsWorld sleepPairWorld;
            sleepPairWorld.setGravity(glm::vec3(0.0f));
            const RigidBodySettings settings;
            const PhysicsBodyId first = sleepPairWorld.createDynamicBody(sphere, glm::vec3(0.0f), settings);
            const PhysicsBodyId second = sleepPairWorld.createDynamicBody(sphere, glm::vec3(0.8f, 0.0f, 0.0f), settings);
            for (int index = 0; index < 120; ++index)
            {
                sleepPairWorld.step(STEP);
            }
            require(sleepPairWorld.isSleeping(first) && sleepPairWorld.isSleeping(second),
                "Overlapping bodies did not settle");
            sleepPairWorld.step(STEP);
            require(sleepPairWorld.lastNarrowphasePairCount() == 0,
                "Sleeping pair still entered narrowphase");
        }

        // 删除动态体后不再参与统计与积分。
        {
            PhysicsWorld removeWorld;
            const PhysicsBodyId body = removeWorld.createDynamicBody(sphere, glm::vec3(0.0f));
            require(removeWorld.destroyBody(body), "Destroying a dynamic body failed");
            require(removeWorld.dynamicBodyCount() == 0 && removeWorld.bodyCount() == 0,
                "Dynamic body was not removed");
        }

        std::cout << "physics_rigidbody_test passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "physics_rigidbody_test failed: " << error.what() << '\n';
        return 1;
    }
}
