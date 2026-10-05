#include "SkinBinding.h"
#include "Scene.h"
#include <stdexcept>

std::vector<glm::mat4> SkinBinding::palette(const Scene &scene, const Transform &meshTransform) const
{
    std::vector<glm::mat4> world;
    world.reserve(joints.size());
    for (auto id : joints)
    {
        const auto *joint = scene.findObject(id);
        if (!joint) { throw std::runtime_error("Skin joint object was removed"); }
        world.push_back(joint->transform.worldMatrix());
    }
    return buildSkinPalette(meshTransform.worldMatrix(), world, inverseBind);
}
