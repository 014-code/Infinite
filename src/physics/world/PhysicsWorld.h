#pragma once

#include "physics/body/PhysicsFilter.h"
#include "physics/body/RigidBody.h"
#include "physics/math/Aabb.h"
#include "physics/math/Ray.h"
#include "physics/math/Raycast.h"
#include "physics/shapes/CollisionShape.h"
#include "physics/world/ShapeCollision.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

// 碰撞体句柄：从1开始递增，0无效，删除后不复用；规则与Scene的对象ID一致。
using PhysicsBodyId = std::uint32_t;

// 物理世界：登记碰撞体、按固定步长推进动态体、提供同步碰撞查询。
//
// 结构参照Godot的"物理服务与场景节点分离"：本类不依赖Scene、Window和OpenGL，
// 场景侧只通过PhysicsBodyComponent借用它。
//
// 支持范围（其余组合明确拒绝，不做近似）：
// - 静态体：球体、盒体、胶囊、无限平面。
// - 动态刚体：首版只支持球体形状；盒体/胶囊的动态体需要OBB对OBB等窄相位，尚未实现。
// - 接触求解：顺序冲量 + 位置修正 + 休眠；不产生接触力矩（不滚动、不翻滚）。
// - 只支持主线程同步调用；没有物理线程、CCD、关节与软体。
class PhysicsWorld
{
public:
    PhysicsWorld();
    PhysicsWorld(const PhysicsWorld &) = delete;
    PhysicsWorld &operator=(const PhysicsWorld &) = delete;
    PhysicsWorld(PhysicsWorld &&) = delete;
    PhysicsWorld &operator=(PhysicsWorld &&) = delete;

    // 重力与固定步长；固定步长默认1/60秒，单帧最多补跑4步。
    void setGravity(const glm::vec3 &gravity);
    const glm::vec3 &gravity() const noexcept;
    void setFixedStep(float seconds);
    float fixedStep() const noexcept;
    void setMaximumSubSteps(int steps);
    int maximumSubSteps() const noexcept;
    void setSolverIterations(int iterations);
    int solverIterations() const noexcept;

    // 累加frameDeltaTime并推进固定步长，返回实际执行的步数。
    // 单帧需要补跑的步数超过上限时丢弃多余时间，避免卡顿后的"螺旋失控"。
    int step(float frameDeltaTime);
    // 丢弃累加器中不足一步的剩余时间；瞬移或重置后使用。
    void resetAccumulator();
    // 渲染插值系数：0表示上一步位姿，1表示当前步位姿。
    float interpolationAlpha() const noexcept;
    std::uint64_t fixedStepCount() const noexcept;
    // 上一步真正进入窄相位的形状对数量。它是诊断值，不是模拟状态：
    // 用于确认扫掠剪枝生效（分离的物体不产生候选对），测试也依赖它。
    std::size_t lastNarrowphasePairCount() const noexcept;

    // 静态体：位置只影响平面的offset，旋转改变平面法线。平面没有有限包围盒。
    PhysicsBodyId createStaticBody(const CollisionShape &shape, const glm::vec3 &position);
    PhysicsBodyId createStaticBody(const CollisionShape &shape, const glm::vec3 &position,
        const glm::quat &rotation, const PhysicsFilter &filter = {});
    // 动态刚体：首版只接受球体；其他形状抛std::invalid_argument。
    PhysicsBodyId createDynamicBody(const CollisionShape &shape, const glm::vec3 &position,
        const RigidBodySettings &settings = {}, const PhysicsFilter &filter = {});

    // 删除不存在的句柄返回false；删除后其他句柄仍有效。
    bool destroyBody(PhysicsBodyId id);
    void clear();
    bool contains(PhysicsBodyId id) const;
    std::size_t bodyCount() const;
    // 全部句柄，按注册顺序；供调试可视化与序列化使用。
    std::vector<PhysicsBodyId> bodyIds() const;
    // 动态体数量；静态体不参与积分与接触求解。
    std::size_t dynamicBodyCount() const;

    // 直接设置位姿（静态体用于移动地形；动态体会同时清零速度并唤醒）。
    bool setBodyTransform(PhysicsBodyId id, const glm::vec3 &position, const glm::quat &rotation);
    std::optional<glm::vec3> bodyPosition(PhysicsBodyId id) const;
    std::optional<glm::quat> bodyRotation(PhysicsBodyId id) const;
    bool isDynamicBody(PhysicsBodyId id) const;
    // 返回形状借用指针；句柄无效返回nullptr，指针在执行增删后不可继续保存。
    const CollisionShape *bodyShape(PhysicsBodyId id) const;
    std::uint32_t bodyLayer(PhysicsBodyId id) const;
    std::optional<glm::mat4> bodyWorldMatrix(PhysicsBodyId id) const;
    // 平面体与无效句柄返回std::nullopt，便于查询代码统一处理。
    std::optional<Aabb> bodyWorldAabb(PhysicsBodyId id) const;

    // 动态体状态：当前步或按插值系数混合后的位姿；静态体与无效句柄返回std::nullopt。
    std::optional<RigidBodyState> bodyState(PhysicsBodyId id) const;
    std::optional<RigidBodyState> interpolatedBodyState(PhysicsBodyId id, float alpha) const;
    bool setBodyVelocity(PhysicsBodyId id, const glm::vec3 &velocity);
    bool setBodyAngularVelocity(PhysicsBodyId id, const glm::vec3 &angularVelocity);
    // 施加冲量并唤醒；零冲量只唤醒不改变速度。
    bool applyImpulse(PhysicsBodyId id, const glm::vec3 &impulse);
    bool wakeBody(PhysicsBodyId id);
    bool isSleeping(PhysicsBodyId id) const;

    // 世界空间射线检测：返回最近命中，maxDistance用于限制有效距离，queryMask筛选对方层。
    struct RaycastResult
    {
        PhysicsBodyId body;
        RaycastHit hit;
        bool dynamic = false;
    };
    std::optional<RaycastResult> raycast(const Ray &ray,
        float maxDistance = std::numeric_limits<float>::max(), std::uint32_t queryMask = 0xFFFFFFFFu) const;

    struct OverlapResult
    {
        PhysicsBodyId body;
        ShapeContact contact;
        // 把查询形状沿该方向移出Body；平面与非平面的法线约定不同，统一在这里归一。
        glm::vec3 pushOutNormal;
        bool dynamic = false;
    };
    // 形状重叠查询：支持与narrowphase相同的形状组合，未支持的组合不产生结果。
    // 平面不能作为查询形状（没有有限范围），调用抛std::invalid_argument。
    std::vector<OverlapResult> overlapShape(const CollisionShape &shape, const glm::vec3 &position,
        const glm::quat &rotation, std::uint32_t queryMask = 0xFFFFFFFFu) const;

    // broadphase候选对：沿X轴扫掠剪枝后按Y/Z区间过滤，并应用碰撞层过滤。
    std::vector<std::pair<PhysicsBodyId, PhysicsBodyId>> broadphasePairs() const;

private:
    struct Body
    {
        // CollisionShape没有默认构造，因此Body也只在创建时一次性构造。
        Body(PhysicsBodyId bodyId, const CollisionShape &bodyShape, const PhysicsFilter &bodyFilter)
            : id(bodyId), shape(bodyShape), filter(bodyFilter) {}

        PhysicsBodyId id = 0;
        CollisionShape shape;
        PhysicsFilter filter;
        bool dynamic = false;
        RigidBodySettings settings;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        // 上一步位姿，供渲染插值使用。
        glm::vec3 previousPosition{0.0f};
        glm::quat previousRotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 velocity{0.0f};
        glm::vec3 angularVelocity{0.0f};
        bool sleeping = false;
        float sleepTimer = 0.0f;
        std::optional<Aabb> worldAabb; // 平面体没有有限包围盒。
    };

    // 一次接触：a为动态体，b可能是动态体或静态体（index为npos表示静态）。
    struct Contact
    {
        std::size_t a = 0;
        std::size_t b = 0;
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f}; // 把a沿该方向推开可分离。
        float penetration = 0.0f;
        float restitution = 0.0f;
        float friction = 0.0f;
    };

    static constexpr std::size_t NO_BODY = std::numeric_limits<std::size_t>::max();

    Body *find(PhysicsBodyId id);
    const Body *find(PhysicsBodyId id) const;
    void refreshWorldAabb(Body &body);
    void stepFixedOnce(float stepSeconds);
    // 扫掠剪枝候选对（按bodies_下标输出，已应用包围盒与层/掩码过滤）。
    // 公开的broadphasePairs()与step的接触收集共用这一份实现，避免两套规则漂移。
    void collectSweepCandidates(std::vector<std::pair<std::size_t, std::size_t>> &candidates) const;
    void collectContacts(std::vector<Contact> &contacts) const;
    void buildContact(std::size_t indexA, std::size_t indexB, std::vector<Contact> &contacts) const;
    void solveVelocity(std::vector<Contact> &contacts);
    void correctPositions(std::vector<Contact> &contacts);
    // 删除体后下标会移动，需要重建id到下标的映射。
    void rebuildIndex();

    std::vector<std::unique_ptr<Body>> bodies_;
    // id到下标的映射：让find与所有按id的访问都是O(1)。增删时同步维护。
    std::unordered_map<PhysicsBodyId, std::size_t> indexById_;
    PhysicsBodyId nextId_ = 1;
    glm::vec3 gravity_{0.0f, -9.81f, 0.0f};
    float fixedStep_ = 1.0f / 60.0f;
    int maximumSubSteps_ = 4;
    int solverIterations_ = 8;
    float accumulator_ = 0.0f;
    std::uint64_t fixedStepCount_ = 0;
    // 上一步进入窄相位的形状对数量（含平面配对）：用于验证broadphase真的省掉了全配对，
    // 以及测试规模相关行为。它不是模拟状态，不参与求解；在const的接触收集里更新。
    mutable std::size_t lastNarrowphasePairCount_ = 0;
};
