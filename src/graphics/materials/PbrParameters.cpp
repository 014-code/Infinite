#include "PbrParameters.h"
#include <cmath>
#include <stdexcept>

void PbrParameters::validate() const
{
    const auto unit = [](float value) { return std::isfinite(value) && value >= 0 && value <= 1; };
    if (!unit(metallic) || !unit(roughness) || !unit(occlusionStrength) || !unit(alphaCutoff) ||
        !std::isfinite(normalScale) || normalScale < 0 || normalScale > 100)
    {
        throw std::invalid_argument("Invalid PBR metallic/roughness/normal/AO/alpha parameters");
    }
    for (int i = 0; i < 3; ++i)
    {
        if (!std::isfinite(emission[i]) || emission[i] < 0 || emission[i] > 1000000)
        {
            throw std::invalid_argument("PBR emission must be finite and in [0, 1000000]");
        }
    }
}
