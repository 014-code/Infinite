#include "TestSupport.h"
#include "common/CameraRelativeInput.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    void requireVecNear(const glm::vec3 &actual, const glm::vec3 &expected, const char *message)
    {
        require(std::abs(actual.x - expected.x) < 0.0001f && std::abs(actual.y - expected.y) < 0.0001f &&
            std::abs(actual.z - expected.z) < 0.0001f, message);
    }
}

int main()
{
    try
    {
        // 视线朝-Z（OpenGL默认观察方向）：屏幕右方是+X。
        // 这条用例就是"左右颠倒"的回归测试：写成cross(up, forward)时D会指向-X。
        requireVecNear(cameraRelativeDirection(glm::vec3(0.0f, 0.0f, -1.0f), 0.0f, 1.0f),
            glm::vec3(1.0f, 0.0f, 0.0f), "Pressing D must move along screen-right (+X when looking down -Z)");
        requireVecNear(cameraRelativeDirection(glm::vec3(0.0f, 0.0f, -1.0f), 0.0f, -1.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f), "Pressing A must move along screen-left");
        requireVecNear(cameraRelativeDirection(glm::vec3(0.0f, 0.0f, -1.0f), 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f), "Pressing W must move along the view direction");
        requireVecNear(cameraRelativeDirection(glm::vec3(0.0f, 0.0f, -1.0f), -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f), "Pressing S must move against the view direction");

        // 视线朝+Z时屏幕右方是-X；视线朝+X时屏幕右方是+Z。
        requireVecNear(cameraRelativeDirection(glm::vec3(0.0f, 0.0f, 1.0f), 0.0f, 1.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f), "Screen-right while looking down +Z is wrong");
        requireVecNear(cameraRelativeDirection(glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f), "Screen-right while looking down +X is wrong");
        requireVecNear(cameraRelativeDirection(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f), "Forward while looking down +X is wrong");

        // 视线带俯仰角时先投影到水平面：结果与纯水平视线一致，且不含Y分量。
        requireVecNear(cameraRelativeDirection(glm::vec3(0.0f, -5.0f, -3.0f), 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f), "Tilted view was not projected onto the horizontal plane");
        requireVecNear(cameraRelativeDirection(glm::vec3(0.0f, -5.0f, -3.0f), 0.0f, 1.0f),
            glm::vec3(1.0f, 0.0f, 0.0f), "Tilted view produced a wrong screen-right");
        requireVecNear(cameraRelativeDirection(glm::vec3(3.0f, -9.0f, 0.0f), 0.0f, 0.0f),
            glm::vec3(0.0f), "No input must produce no direction");

        // 斜向输入保留强度（不在这里归一化）；长度与输入向量长度一致。
        const glm::vec3 diagonal = cameraRelativeDirection(glm::vec3(0.0f, 0.0f, -1.0f), 1.0f, 1.0f);
        require(std::abs(glm::length(diagonal) - std::sqrt(2.0f)) < 0.0001f,
            "Diagonal input must keep its magnitude");
        requireVecNear(diagonal, glm::vec3(1.0f, 0.0f, -1.0f), "Diagonal direction is wrong");

        // 完全垂直俯视没有水平视线方向，必须明确报错而不是给出任意方向。
        expectThrow<std::invalid_argument>([] {
            cameraRelativeDirection(glm::vec3(0.0f, -1.0f, 0.0f), 1.0f, 0.0f); },
            "Vertical view direction was accepted");
        expectThrow<std::invalid_argument>([] {
            cameraRelativeDirection(glm::vec3(0.0f, 0.0f, -1.0f),
                std::numeric_limits<float>::quiet_NaN(), 0.0f); },
            "NaN input was accepted");

        std::cout << "camera_relative_input_test passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "camera_relative_input_test failed: " << error.what() << '\n';
        return 1;
    }
}
