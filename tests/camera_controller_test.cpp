#include "TestSupport.h"
#include "common/FreeCameraController.h"

#include <iostream>
#include <limits>

namespace
{
    bool near(const glm::vec3 &left, const glm::vec3 &right)
    {
        return glm::length(left - right) < 0.0001f;
    }
}

int main()
{
    try
    {
        Camera camera;
        FreeCameraController controller;
        controller.apply(camera);
        InputState state;
        state.keyEvent(Key::W, true);
        controller.update(camera, FreeCameraController::sample(state), 1.0f);
        require(near(camera.position(), {0, 0, 0}), "W did not move forward by 3 units");
        state.focusLost();
        state.beginFrame();
        controller.update(camera, FreeCameraController::sample(state), 1.0f);
        require(near(camera.position(), {0, 0, 0}), "Release left camera moving");

        // 多轴输入先归一化，斜向移动和直行速度相同；Shift只修改速度。
        state.keyEvent(Key::D, true);
        state.keyEvent(Key::Space, true);
        state.keyEvent(Key::RightShift, true);
        controller.update(camera, FreeCameraController::sample(state), 0.5f);
        require(std::abs(glm::length(camera.position()) - 4.5f) < 0.0001f, "Diagonal/fast speed wrong");
        state.focusLost();
        state.beginFrame();
        state.keyEvent(Key::RightControl, true);
        require(FreeCameraController::sample(state).movement.y == -1, "Ctrl mapping wrong");

        FreeCameraInput controls;
        controls.reset = true;
        controller.update(camera, controls, 0.1f);
        require(near(camera.position(), {0, 0, 3}), "R reset position failed");
        // 展示场景使用自定义开场构图，移动后R也必须回到该构图，而不是旧默认原点。
        FreeCameraSettings gallery;
        gallery.startPosition = {7, 5, 12};
        gallery.startYaw = -2.0f;
        gallery.startPitch = -0.4f;
        FreeCameraController galleryController(gallery);
        Camera galleryCamera;
        galleryController.apply(galleryCamera);
        const auto galleryTarget = galleryCamera.target();
        FreeCameraInput move;
        move.movement = {0, 0, 1};
        galleryController.update(galleryCamera, move, 1);
        galleryController.update(galleryCamera, controls, 0.1f);
        require(near(galleryCamera.position(), gallery.startPosition) &&
            near(galleryCamera.target(), galleryTarget), "Configured camera reset failed");
        gallery.startPitch = glm::pi<float>();
        expectThrow<std::invalid_argument>([&] { FreeCameraController bad(gallery); }, "Invalid initial pitch accepted");
        controls.reset = false;
        controls.mouseDelta = {100, 50};
        const float originalYaw = controller.yaw();
        controller.update(camera, controls, 0.1f);
        require(controller.yaw() == originalYaw, "Mouse moved without right button");
        controls.looking = true;
        controller.update(camera, controls, 0.1f);
        require(controller.yaw() == originalYaw, "Initial right press jumped");
        controller.update(camera, controls, 0.01f);
        require(std::abs(controller.yaw() - originalYaw - 0.25f) < 0.0001f, "Look sensitivity/direction wrong");
        require(std::abs(controller.pitch() + 0.125f) < 0.0001f, "Pitch direction wrong");

        Camera otherCamera;
        FreeCameraController other;
        other.update(otherCamera, controls, 0.1f);
        other.update(otherCamera, controls, 0.5f);
        require(std::abs(other.yaw() - controller.yaw()) < 0.0001f, "Mouse motion multiplied by deltaTime");
        controls.mouseDelta.y = -100000;
        controller.update(camera, controls, 0.1f);
        require(controller.pitch() < glm::half_pi<float>() && controller.pitch() > 1.5f, "Pitch not clamped");

        const auto position = controller.position();
        const auto yaw = controller.yaw();
        controls.focused = false;
        controls.movement = {1, 1, 1};
        controller.update(camera, controls, 1);
        require(near(controller.position(), position) && controller.yaw() == yaw, "Unfocused input moved camera");
        controls.focused = true;
        controller.update(camera, controls, 0);
        require(near(controller.position(), position) && controller.yaw() == yaw, "Resume dt0 moved camera");
        controls.movement = {0, 0, 0};
        controller.update(camera, controls, 0.1f);
        require(controller.yaw() == yaw, "Refocus first mouse event jumped");

        expectThrow<std::invalid_argument>([&] { controller.update(camera, controls, -1); }, "Negative dt accepted");
        controls.mouseDelta.x = std::numeric_limits<double>::infinity();
        expectThrow<std::invalid_argument>([&] { controller.update(camera, controls, 1); }, "Infinite mouse accepted");
        require(near(controller.position(), position), "Invalid input changed controller");
        FreeCameraSettings invalid;
        invalid.fastMultiplier = 0.5f;
        expectThrow<std::invalid_argument>([&] { FreeCameraController bad(invalid); }, "Invalid speed accepted");
        std::cout << "Free camera mapping, motion, focus and reset passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
