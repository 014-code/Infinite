#include "particles/ParticleSystem.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char *message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }
}

int main()
{
    try
    {
        ParticleEmitterSettings settings;
        settings.maxParticles = 4;
        settings.emissionRate = 4.0f;
        settings.lifetime = 1.0f;
        settings.initialVelocityMin = settings.initialVelocityMax = {0.0f, 1.0f, 0.0f};
        settings.acceleration = {0.0f, -1.0f, 0.0f};
        ParticleSystem particles(settings);
        particles.setOrigin({1.0f, 2.0f, 3.0f});
        particles.update(0.5f);
        require(particles.activeCount() == 2, "Particle emission rate was not integrated");
        require(particles.particles().front().position.y > 2.0f,
            "Particle movement was not integrated");
        require(particles.particles().front().color.w < 1.0f,
            "Particle color did not interpolate");
        particles.update(0.6f);
        require(particles.activeCount() == 2, "Expired particles were not replaced consistently");
        particles.clear();
        require(particles.activeCount() == 0, "Particle clear failed");
        bool failed = false;
        try { ParticleEmitterSettings invalid; invalid.lifetime = 0.0f; ParticleSystem bad(invalid); }
        catch (const std::invalid_argument &) { failed = true; }
        require(failed, "Invalid particle settings were accepted");
        std::cout << "Particle system passed\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
