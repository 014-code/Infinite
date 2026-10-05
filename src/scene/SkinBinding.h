#pragma once

#include "animation/Skin.h"
#include "scene/GameObject.h"

class Scene;
class Transform;

// 场景实例中的关节绑定。只保存ObjectId，不借用可能被删除的GameObject指针。
// 一个模型实例可以在多个primitive间共享此绑定，不与另一实例共享关节ID。
struct SkinBinding
{
    std::vector<ObjectId> joints;
    std::vector<glm::mat4> inverseBind;
    std::vector<glm::mat4> palette(const Scene &scene, const Transform &meshTransform) const;
};
