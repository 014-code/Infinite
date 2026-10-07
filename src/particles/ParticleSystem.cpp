#include "ParticleSystem.h"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    bool finite(const glm::vec3 &value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    bool finite(const glm::vec4 &value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
            std::isfinite(value.z) && std::isfinite(value.w);
    }
}

ParticleSystem::ParticleSystem(const ParticleEmitterSettings &settings)
    : settings_(settings), random_(settings.randomSeed)
{
    validate(settings_);
    particles_.reserve(settings_.maxParticles);
}

void ParticleSystem::validate(const ParticleEmitterSettings &settings) const
{
    if (settings.maxParticles == 0 || !std::isfinite(settings.emissionRate) ||
        settings.emissionRate < 0.0f || !std::isfinite(settings.lifetime) ||
        settings.lifetime <= 0.0f || !finite(settings.initialVelocityMin) ||
        !finite(settings.initialVelocityMax) || !finite(settings.acceleration) ||
        !finite(settings.startColor) || !finite(settings.endColor) ||
        !std::isfinite(settings.startSize) || settings.startSize < 0.0f ||
        !std::isfinite(settings.endSize) || settings.endSize < 0.0f)
    {
        throw std::invalid_argument("Invalid particle emitter settings");
    }
    if (settings.initialVelocityMin.x > settings.initialVelocityMax.x ||
        settings.initialVelocityMin.y > settings.initialVelocityMax.y ||
        settings.initialVelocityMin.z > settings.initialVelocityMax.z)
    {
        throw std::invalid_argument("Particle velocity minimum must not exceed maximum");
    }
}

void ParticleSystem::setSettings(const ParticleEmitterSettings &settings)
{
    validate(settings);
    settings_ = settings;
    random_.seed(settings.randomSeed);
    emissionAccumulator_ = 0.0f;
    particles_.clear();
    particles_.reserve(settings_.maxParticles);
}

void ParticleSystem::spawnOne()
{
    if (particles_.size() >= settings_.maxParticles)
    {
        return;
    }
    std::uniform_real_distribution<float> x(settings_.initialVelocityMin.x, settings_.initialVelocityMax.x);
    std::uniform_real_distribution<float> y(settings_.initialVelocityMin.y, settings_.initialVelocityMax.y);
    std::uniform_real_distribution<float> z(settings_.initialVelocityMin.z, settings_.initialVelocityMax.z);
    Particle particle;
    particle.position = origin_;
    particle.velocity = {x(random_), y(random_), z(random_)};
    particle.color = settings_.startColor;
    particle.size = settings_.startSize;
    particle.lifetime = settings_.lifetime;
    particles_.push_back(particle);
}

void ParticleSystem::emit(std::size_t count)
{
    for (std::size_t index = 0; index < count && particles_.size() < settings_.maxParticles; ++index)
    {
        spawnOne();
    }
}

void ParticleSystem::update(float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
    {
        throw std::invalid_argument("Particle delta time must be finite and non-negative");
    }

    if (settings_.looping && settings_.emissionRate > 0.0f)
    {
        emissionAccumulator_ += settings_.emissionRate * deltaTime;
        const std::size_t count = static_cast<std::size_t>(emissionAccumulator_);
        emissionAccumulator_ -= static_cast<float>(count);
        emit(count);
    }

    for (Particle &particle : particles_)
    {
        particle.age += deltaTime;
        particle.velocity += settings_.acceleration * deltaTime;
        particle.position += particle.velocity * deltaTime;
        const float progress = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
        particle.color = glm::mix(settings_.startColor, settings_.endColor, progress);
        particle.size = settings_.startSize + (settings_.endSize - settings_.startSize) * progress;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
        [](const Particle &particle) { return particle.age >= particle.lifetime; }), particles_.end());
}

void ParticleSystem::clear() noexcept
{
    particles_.clear();
    emissionAccumulator_ = 0.0f;
}
