#include "TestSupport.h"
#include "graphics/camera/Camera.h"

#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    try
    {
        Camera camera;
        const auto perspective = camera.projectionMatrix(2);
        require(perspective[0][0] < perspective[1][1], "Perspective ignored aspect ratio");
        camera.setView({1, 2, 3}, {1, 2, 2});
        const auto origin = camera.viewMatrix() * glm::vec4(camera.position(), 1);
        require(std::abs(origin.x) + std::abs(origin.y) + std::abs(origin.z) < 0.0001f, "View translation wrong");
        camera.setOrthographic(4, -1, 20);
        const auto ortho = camera.projectionMatrix(2);
        require(ortho[0][0] == 0.25f && ortho[1][1] == 0.5f, "Orthographic size wrong");
        expectThrow<std::invalid_argument>([&] { camera.setPerspective(180, 1, 20); }, "Invalid fov accepted");
        require(camera.projectionMode() == Camera::ProjectionMode::Orthographic, "Failed setter mutated camera");
        camera.setPerspective(60, 0.1f, 50);
        expectThrow<std::invalid_argument>([&] { camera.projectionMatrix(0); }, "Invalid aspect accepted");
        expectThrow<std::invalid_argument>([&] { camera.setView({0, 0, 0}, {0, 0, 0}); }, "Degenerate view accepted");
        expectThrow<std::invalid_argument>([&] { camera.setView({0, 0, 0}, {0, 1, 0}); }, "Parallel up accepted");
        expectThrow<std::invalid_argument>([&] { camera.setOrthographic(0, 1, 20); }, "Zero height accepted");
        expectThrow<std::invalid_argument>([&] { camera.setPerspective(60, 20, 1); }, "Reversed clip accepted");
        expectThrow<std::invalid_argument>([&] { camera.projectionMatrix(std::numeric_limits<float>::infinity()); }, "Infinite aspect accepted");
        std::cout << "Camera passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
