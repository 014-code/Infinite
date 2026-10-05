#include "Skin.h"
#include <glm/matrix.hpp>
#include <cmath>
#include <stdexcept>

namespace
{
    void finite(const glm::mat4 &matrix)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            { if (!std::isfinite(matrix[column][row])) { throw std::invalid_argument("Non-finite skin matrix"); } }
        }
    }
}

std::vector<glm::mat4> buildSkinPalette(const glm::mat4 &meshWorld,
    const std::vector<glm::mat4> &jointWorld, const std::vector<glm::mat4> &inverseBind)
{
    if (jointWorld.empty() || jointWorld.size() != inverseBind.size())
    { throw std::invalid_argument("Skin joint and inverse-bind counts differ or are empty"); }
    finite(meshWorld);
    const auto inverseMesh = glm::inverse(meshWorld);
    finite(inverseMesh);
    std::vector<glm::mat4> result;
    result.reserve(jointWorld.size());
    for (std::size_t joint = 0; joint < jointWorld.size(); ++joint)
    {
        finite(jointWorld[joint]); finite(inverseBind[joint]);
        auto matrix = inverseMesh * jointWorld[joint] * inverseBind[joint];
        finite(matrix);
        result.push_back(matrix);
    }
    return result;
}
