#include "FlatGroundMovement.h"

#include "math/Transform.h"

#include <glm/geometric.hpp>
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace PlayerExample
{
    namespace
    {
        void validateSettings(const FlatGroundSettings &settings)
        {
            for (const float value : {settings.moveSpeed, settings.sprintMultiplier,
                settings.jumpSpeed, settings.gravity, settings.maxFallSpeed, settings.groundHeight})
            {
                if (!std::isfinite(value))
                {
                    throw std::invalid_argument("Flat-ground example settings must be finite");
                }
            }
            if (settings.moveSpeed <= 0.0f || settings.sprintMultiplier < 1.0f ||
                settings.jumpSpeed < 0.0f || settings.gravity >= 0.0f || settings.maxFallSpeed <= 0.0f)
            {
                throw std::invalid_argument("Flat-ground example settings are invalid");
            }
        }

        void validateTransform(const Transform &transform)
        {
            if (!std::isfinite(transform.position.x) || !std::isfinite(transform.position.y) ||
                !std::isfinite(transform.position.z))
            {
                throw std::invalid_argument("Player example position must be finite");
            }
            if (transform.parent() != nullptr)
            { throw std::invalid_argument("Flat-ground example requires a root Transform"); }
        }
    }

    FlatGroundMovement::FlatGroundMovement(const FlatGroundSettings &settings)
        : settings_(settings)
    {
        validateSettings(settings_);
    }

    void FlatGroundMovement::update(Transform &transform, const PlayerCommand &command, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
        {
            throw std::invalid_argument("Flat-ground example deltaTime must be finite and nonnegative");
        }
        validateTransform(transform);
        if (!std::isfinite(command.movement.x) || !std::isfinite(command.movement.y))
        {
            throw std::invalid_argument("Player command movement must be finite");
        }
        // Application暂停恢复首帧传0；这帧不消耗跳跃、不改变朝向或内部竖直状态。
        if (deltaTime == 0.0f) { return; }

        // 用double求长度，极大但有限的float输入不会在归一化时溢出。
        glm::dvec2 movement(command.movement);
        if (glm::length(movement) > 1.0) { movement = glm::normalize(movement); }

        const double speed = double(settings_.moveSpeed) *
            (command.sprint ? settings_.sprintMultiplier : 1.0f);
        // 使用局部候选状态计算，所有检查通过后才提交；异常不会留下移动一半的玩家。
        glm::dvec3 position(transform.position);
        position.x += movement.x * speed * deltaTime;
        position.z += movement.y * speed * deltaTime;
        bool grounded = position.y <= settings_.groundHeight;
        double velocity = verticalVelocity_;
        if (grounded)
        {
            position.y = settings_.groundHeight;
            velocity = 0.0;
        }
        if (grounded && command.jumpPressed && settings_.jumpSpeed > 0.0f)
        {
            grounded = false;
            velocity = settings_.jumpSpeed;
        }
        if (!grounded)
        {
            // 恒定重力使用s=v*t+0.5*g*t*t，避免不同帧率产生明显不同的跳跃高度。
            // 达到最大下落速度后，余下时间按匀速处理，不能先截速度再算整帧位移。
            const double acceleratingTime = std::clamp(
                (-double(settings_.maxFallSpeed) - velocity) / settings_.gravity, 0.0, double(deltaTime));
            position.y += velocity * acceleratingTime + 0.5 * settings_.gravity * acceleratingTime * acceleratingTime;
            velocity = std::max(velocity + settings_.gravity * acceleratingTime, -double(settings_.maxFallSpeed));
            position.y += velocity * (double(deltaTime) - acceleratingTime);
            if (position.y <= settings_.groundHeight)
            {
                position.y = settings_.groundHeight;
                velocity = 0.0;
                grounded = true;
            }
        }
        for (int component = 0; component < 3; ++component)
        {
            if (!std::isfinite(position[component]) || std::abs(position[component]) > std::numeric_limits<float>::max())
            { throw std::invalid_argument("Player movement overflowed its transform"); }
        }
        // 移动规则不决定模型朝向；FPS、俯视角等玩法可以在应用中采用不同的转向方式。
        transform.position = glm::vec3(position);
        verticalVelocity_ = static_cast<float>(velocity);
        grounded_ = grounded;
    }

    void FlatGroundMovement::reset(Transform &transform)
    {
        validateTransform(transform);
        transform.position.y = settings_.groundHeight;
        verticalVelocity_ = 0.0f;
        grounded_ = true;
    }

    bool FlatGroundMovement::grounded() const noexcept { return grounded_; }
    float FlatGroundMovement::verticalVelocity() const noexcept { return verticalVelocity_; }
}
