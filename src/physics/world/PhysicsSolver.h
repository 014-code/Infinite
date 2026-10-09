#pragma once

#include "physics/world/PhysicsWorld.h"

#include <cstddef>
#include <limits>
#include <vector>

// PhysicsSolver负责推进PhysicsWorld的一次固定物理步。
//
// 求解器不拥有Body，也不暴露Contact给应用层；PhysicsWorld只负责固定步计时
// 和公开查询，所有积分、接触求解与休眠规则在这里集中维护。
class PhysicsSolver final
{
public:
    // 按固定顺序执行一次完整模拟步。调用者必须已经在外部执行固定步回调。
    static void step(PhysicsWorld &world, float stepSeconds);

private:
    struct Contact
    {
        // a始终是动态体；b为另一个动态体，或kNoBody表示静态体。
        std::size_t a = 0;
        std::size_t b = 0;
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f};
        float penetration = 0.0f;
        float restitution = 0.0f;
        float friction = 0.0f;
    };

    static constexpr std::size_t kNoBody = std::numeric_limits<std::size_t>::max();

    static void collectContacts(const PhysicsWorld &world, std::vector<Contact> &contacts);
    static void buildContact(const PhysicsWorld &world, std::size_t indexA,
        std::size_t indexB, std::vector<Contact> &contacts);
    static void solveVelocity(PhysicsWorld &world, std::vector<Contact> &contacts);
    static void correctPositions(PhysicsWorld &world, std::vector<Contact> &contacts);
};
