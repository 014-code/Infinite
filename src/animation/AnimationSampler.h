#pragma once

#include "AnimationClip.h"
#include <glm/gtc/quaternion.hpp>

// 不包含父子指针的局部姿态快照；采样不会修改模型资源或建立Transform链接。
struct LocalPose
{
    glm::vec3 position{0};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 scale{1};
};

class AnimationSampler final
{
public:
    // 从restPose开始，只覆盖片段包含的通道。首帧前/末帧后保持端点；不负责循环。
    // 返回完整姿态后由播放器统一提交，采样失败不会留下半更新的场景。
    static std::vector<LocalPose> sample(const AnimationClip &clip, double seconds,
        const std::vector<LocalPose> &restPose);
};
