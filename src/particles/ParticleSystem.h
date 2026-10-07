#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

struct Particle
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec4 color{1.0f};
    float size = 1.0f;
    float age = 0.0f;
    float lifetime = 1.0f;
};

// 粒子发射参数。第一阶段只处理CPU生命周期和运动，渲染器可以按需读取particles()。
struct ParticleEmitterSettings
{
    std::size_t maxParticles = 256;
    float emissionRate = 20.0f;
    float lifetime = 1.0f;
    glm::vec3 initialVelocityMin{-0.5f, 1.0f, -0.5f};
    glm::vec3 initialVelocityMax{0.5f, 2.0f, 0.5f};
    glm::vec3 acceleration{0.0f, -2.0f, 0.0f};
    glm::vec4 startColor{1.0f, 0.75f, 0.25f, 1.0f};
    glm::vec4 endColor{1.0f, 0.15f, 0.02f, 0.0f};
    float startSize = 0.12f;
    float endSize = 0.02f;
    bool looping = true;
    std::uint32_t randomSeed = 1;
};

class ParticleSystem final
{
public:
    explicit ParticleSystem(const ParticleEmitterSettings &settings = {});

    void setOrigin(const glm::vec3 &origin) noexcept { origin_ = origin; }
    const glm::vec3 &origin() const noexcept { return origin_; }

    void update(float deltaTime);
    void emit(std::size_t count);
    void clear() noexcept;

    const ParticleEmitterSettings &settings() const noexcept { return settings_; }
    void setSettings(const ParticleEmitterSettings &settings);
    const std::vector<Particle> &particles() const noexcept { return particles_; }
    std::size_t activeCount() const noexcept { return particles_.size(); }

private:
    void validate(const ParticleEmitterSettings &settings) const;
    void spawnOne();

    ParticleEmitterSettings settings_;
    glm::vec3 origin_{0.0f};
    std::vector<Particle> particles_;
    float emissionAccumulator_ = 0.0f;
    std::mt19937 random_;
};
