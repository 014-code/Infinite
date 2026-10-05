#pragma once

#include "graphics/camera/Camera.h"
#include "input/Input.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

struct FreeCameraSettings
{
    float moveSpeed = 3.0f;
    float fastMultiplier = 3.0f;
    float mouseSensitivity = 0.0025f; // 每个逻辑像素对应的弧度，不再乘deltaTime。
    float maximumPitch = glm::radians(89.0f);
    // 示例可指定开场构图；R复位也回到这组位置/角度。默认值保持旧示例行为。
    glm::vec3 startPosition{0.0f, 0.0f, 3.0f};
    float startYaw = -glm::half_pi<float>();
    float startPitch = 0.0f;
};

// 将本帧意图与窗口分开，方便直接验证运动数学，不必模拟系统键盘。
struct FreeCameraInput
{
    glm::vec3 movement{0.0f}; // x向右、y向上、z向前；最终会归一化。
    glm::dvec2 mouseDelta{0.0};
    bool looking = false;
    bool fast = false;
    bool reset = false;
    bool focused = true;
};

// 示例层的控制策略，不进入引擎库：WASD水平移动，Space/Ctrl升降，Shift加速。
// 右键拖动观察，不锁定鼠标；R回到初始位置。Camera本身仍只负责视图/投影。
class FreeCameraController
{
public:
    explicit FreeCameraController(const FreeCameraSettings &settings = {})
        : settings_(settings), position_(settings.startPosition),
          yaw_(settings.startYaw), pitch_(settings.startPitch)
    {
        if (!std::isfinite(settings.moveSpeed) || settings.moveSpeed <= 0.0f ||
            !std::isfinite(settings.fastMultiplier) || settings.fastMultiplier < 1.0f ||
            !std::isfinite(settings.mouseSensitivity) || settings.mouseSensitivity <= 0.0f ||
            !std::isfinite(settings.maximumPitch) || settings.maximumPitch <= 0.0f ||
            settings.maximumPitch >= glm::half_pi<float>() ||
            !finite(settings.startPosition) || !std::isfinite(settings.startYaw) ||
            !std::isfinite(settings.startPitch) || std::abs(settings.startPitch) > settings.maximumPitch)
        {
            throw std::invalid_argument("Free camera settings are invalid");
        }
    }

    void apply(Camera &camera) const
    {
        camera.setView(position_, position_ + front(yaw_, pitch_), {0.0f, 1.0f, 0.0f});
    }

    // Input和纯CPU InputState提供同名查询，共用这一小段映射，测试能覆盖真实按键方案。
    template<class InputSource>
    static FreeCameraInput sample(const InputSource &input, bool focused = true)
    {
        FreeCameraInput result;
        result.movement.x = float(input.isKeyDown(Key::D)) - float(input.isKeyDown(Key::A));
        result.movement.y = float(input.isKeyDown(Key::Space)) -
            float(input.isKeyDown(Key::LeftControl) || input.isKeyDown(Key::RightControl));
        result.movement.z = float(input.isKeyDown(Key::W)) - float(input.isKeyDown(Key::S));
        result.mouseDelta = input.mouseDelta();
        result.looking = input.isMouseButtonDown(MouseButton::Right);
        result.fast = input.isKeyDown(Key::LeftShift) || input.isKeyDown(Key::RightShift);
        result.reset = input.wasKeyPressed(Key::R);
        result.focused = focused;
        return result;
    }

    void update(Camera &camera, const Input &input, float deltaTime, bool focused)
    {
        update(camera, sample(input, focused), deltaTime);
    }

    void update(Camera &camera, const FreeCameraInput &input, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f || !finite(input.movement) ||
            !std::isfinite(input.mouseDelta.x) || !std::isfinite(input.mouseDelta.y))
        {
            throw std::invalid_argument("Free camera input must be finite with nonnegative deltaTime");
        }
        if (!input.focused)
        {
            looking_ = false;
            return;
        }
        if (input.reset)
        {
            position_ = settings_.startPosition;
            yaw_ = settings_.startYaw;
            pitch_ = settings_.startPitch;
            looking_ = false;
            apply(camera);
            return;
        }
        // Application首帧和暂停恢复首帧传0；不要把恢复前积累的鼠标位移当成一次转向。
        if (deltaTime == 0.0f)
        {
            looking_ = false;
            return;
        }

        float yaw = yaw_;
        float pitch = pitch_;
        if (input.looking && looking_)
        {
            // 用double计算并检查溢出，再把yaw约束在一周内，防止长时间操作丢失精度。
            const double nextYaw = double(yaw) + input.mouseDelta.x * settings_.mouseSensitivity;
            const double nextPitch = double(pitch) - input.mouseDelta.y * settings_.mouseSensitivity;
            if (!std::isfinite(nextYaw) || !std::isfinite(nextPitch))
            {
                throw std::invalid_argument("Free camera mouse motion overflow");
            }
            yaw = static_cast<float>(std::remainder(nextYaw, glm::two_pi<double>()));
            pitch = static_cast<float>(std::clamp(nextPitch,
                -double(settings_.maximumPitch), double(settings_.maximumPitch)));
        }
        // 刚按右键的第一帧忽略已有位移，避免点击前的普通鼠标移动导致镜头突然转向。
        const glm::dvec3 forward(std::cos(yaw), 0.0, std::sin(yaw));
        const glm::dvec3 right = glm::cross(forward, glm::dvec3(0.0, 1.0, 0.0));
        glm::dvec3 direction = right * double(input.movement.x) +
            glm::dvec3(0.0, input.movement.y, 0.0) + forward * double(input.movement.z);
        glm::vec3 position = position_;
        if (glm::length(direction) > 0.0)
        {
            const double speed = double(settings_.moveSpeed) * (input.fast ? settings_.fastMultiplier : 1.0);
            position = glm::vec3(glm::dvec3(position_) +
                glm::normalize(direction) * speed * double(deltaTime));
            if (!finite(position))
            {
                throw std::invalid_argument("Free camera position overflow");
            }
        }
        // 先让Camera验证新视图，成功后才提交控制器状态；失败不会留下半更新状态。
        camera.setView(position, position + front(yaw, pitch), {0.0f, 1.0f, 0.0f});
        position_ = position;
        yaw_ = yaw;
        pitch_ = pitch;
        looking_ = input.looking;
    }

    const glm::vec3 &position() const
    {
        return position_;
    }

    float yaw() const
    {
        return yaw_;
    }

    float pitch() const
    {
        return pitch_;
    }

private:
    static bool finite(const glm::vec3 &value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    static glm::vec3 front(float yaw, float pitch)
    {
        const float horizontal = std::cos(pitch);
        return glm::normalize(glm::vec3(std::cos(yaw) * horizontal,
            std::sin(pitch), std::sin(yaw) * horizontal));
    }

    FreeCameraSettings settings_;
    glm::vec3 position_{0.0f, 0.0f, 3.0f};
    float yaw_ = -glm::half_pi<float>();
    float pitch_ = 0.0f;
    bool looking_ = false;
};
