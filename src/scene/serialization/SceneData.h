#pragma once

#include "scene/serialization/PrimitiveSerialization.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scene_serialization_detail
{
    constexpr int kSceneFormatVersion = 4;
    constexpr std::size_t kMaximumObjectCount = 100000;
    constexpr std::size_t kMaximumObjectNameLength = 4096;

    // 这是纯数据中间结果，解析和构建阶段都不直接保存GPU句柄。
    // parentIndex引用文件中的对象顺序（从0开始），不是fileId；-1表示没有父节点。
    struct ObjectData
    {
        std::uint64_t fileId = 0;
        long long parentIndex = -1;
        std::string name;
        bool active = true;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        glm::vec3 sortOrigin{0.0f};
        std::filesystem::path meshPath;
        std::filesystem::path materialPath;
        std::optional<scene_serialization::PrimitiveState> primitive;
    };

    using ObjectList = std::vector<ObjectData>;
}
