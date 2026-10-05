#include "TestSupport.h"
#include "math/Transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace
{
    void requireNear(float actual, float expected, const char *message)
    {
        require(std::abs(actual - expected) < 0.0001f, message);
    }

    glm::vec3 worldOrigin(const Transform &transform)
    {
        return glm::vec3(transform.worldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    }
}

int main()
{
    try
    {
        Transform root;
        Transform child;
        Transform grandchild;
        root.position = glm::vec3(10.0f, 0.0f, 0.0f);
        child.position = glm::vec3(2.0f, 0.0f, 0.0f);
        grandchild.position = glm::vec3(1.0f, 0.0f, 0.0f);

        child.setParent(&root);
        grandchild.setParent(&child);
        require(child.parent() == &root && grandchild.parent() == &child, "Parent link was not stored");
        require(root.children().size() == 1 && root.children().front() == &child,
            "Root child list is incorrect");
        require(child.children().size() == 1 && child.children().front() == &grandchild,
            "Nested child list is incorrect");
        requireNear(worldOrigin(root).x, 10.0f, "Root world position is incorrect");
        requireNear(worldOrigin(child).x, 12.0f, "Child world position is incorrect");
        requireNear(worldOrigin(grandchild).x, 13.0f, "Grandchild world position is incorrect");

        root.scale = glm::vec3(2.0f);
        requireNear(worldOrigin(child).x, 14.0f, "Parent scale did not affect child");
        root.setEulerAngles({0.0f, 1.57079632679f, 0.0f});
        requireNear(worldOrigin(child).x, 10.0f, "Parent rotation changed the wrong axis");
        requireNear(worldOrigin(child).z, -4.0f, "Parent rotation was not inherited");
        const glm::vec3 childWorldOrigin = worldOrigin(child);
        require(std::isfinite(childWorldOrigin.x) && std::isfinite(childWorldOrigin.y) &&
            std::isfinite(childWorldOrigin.z), "World matrix produced a non-finite value");
        requireNear(root.modelMatrix()[3].x, root.worldMatrix()[3].x, "Legacy modelMatrix changed semantics");

        // 非单轴旋转应与旧版Rx * Ry * Rz矩阵保持一致；四元数内部归一化后
        // 即使输入长度不是1，也不能让物体产生额外缩放。
        Transform compound;
        const glm::vec3 angles(0.2f, 0.4f, -0.3f);
        compound.setEulerAngles(angles);
        glm::mat4 legacyMatrix(1.0f);
        legacyMatrix = glm::rotate(legacyMatrix, angles.x, glm::vec3(1, 0, 0));
        legacyMatrix = glm::rotate(legacyMatrix, angles.y, glm::vec3(0, 1, 0));
        legacyMatrix = glm::rotate(legacyMatrix, angles.z, glm::vec3(0, 0, 1));
        const glm::vec4 probe(0.3f, -0.5f, 0.7f, 1.0f);
        const glm::vec4 actual = compound.localMatrix() * probe;
        const glm::vec4 expected = legacyMatrix * probe;
        requireNear(actual.x, expected.x, "Compound rotation changed X axis");
        requireNear(actual.y, expected.y, "Compound rotation changed Y axis");
        requireNear(actual.z, expected.z, "Compound rotation changed Z axis");
        requireNear(compound.eulerAngles().x, angles.x, "Euler X conversion failed");
        requireNear(compound.eulerAngles().y, angles.y, "Euler Y conversion failed");
        requireNear(compound.eulerAngles().z, angles.z, "Euler Z conversion failed");
        compound.setRotation(compound.rotation() * 2.0f);
        requireNear(glm::length(compound.rotation()), 1.0f, "Quaternion was not normalized");
        expectThrow<std::invalid_argument>([&] { compound.setRotation(glm::quat(0, 0, 0, 0)); },
            "Zero quaternion was accepted");
        expectThrow<std::invalid_argument>([&] {
            compound.setEulerAngles({std::numeric_limits<float>::quiet_NaN(), 0, 0});
        }, "Non-finite Euler angle was accepted");

        child.setParent(nullptr);
        require(child.parent() == nullptr && root.children().empty(), "Unparenting did not update links");
        requireNear(worldOrigin(child).x, 2.0f, "Unparented child kept an unexpected world transform");

        expectThrow<std::invalid_argument>([&] { root.setParent(&root); }, "Self-parenting was accepted");
        root.setParent(&grandchild);
        expectThrow<std::invalid_argument>([&] { grandchild.setParent(&root); }, "Cyclic parenting was accepted");
        root.setParent(nullptr);

        auto dynamicParent = std::make_unique<Transform>();
        Transform survivingChild;
        survivingChild.setParent(dynamicParent.get());
        dynamicParent.reset();
        require(survivingChild.parent() == nullptr, "Destroyed parent left a dangling link");

        Transform temporaryParent;
        {
            Transform temporaryChild;
            temporaryChild.setParent(&temporaryParent);
            require(temporaryParent.children().size() == 1, "Temporary child was not registered");
        }
        require(temporaryParent.children().empty(), "Destroyed child remained in parent list");

        std::cout << "Transform hierarchy passed: local/world matrices, links, cycles and lifetime rules\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Transform hierarchy failed: " << error.what() << '\n';
        return 1;
    }
}
