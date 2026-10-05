#pragma once

#include "AnimationSampler.h"

// 两份姿态必须针对同一模型节点顺序。混合器只处理CPU数据，不访问Scene或OpenGL。
// weight=0完全使用from，weight=1完全使用to；旋转始终走四元数最短路径。
class AnimationBlender final
{
public:
    static std::vector<LocalPose> blend(const std::vector<LocalPose> &from,
        const std::vector<LocalPose> &to, float weight);
};
