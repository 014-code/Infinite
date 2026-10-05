#include "graphics/rendering/RenderItem.h"

#include "math/Transform.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

float transparentViewDepth(const glm::mat4 &worldMatrix, const glm::vec3 &sortOrigin,
    const glm::mat4 &viewMatrix)
{
    const float depth = (viewMatrix * worldMatrix * glm::vec4(sortOrigin, 1.0f)).z;
    if (!std::isfinite(depth))
    {
        throw std::invalid_argument("Transparent item requires a transform with finite view depth");
    }
    return depth;
}

void sortTransparentBackToFront(std::vector<RenderItem> &items, const glm::mat4 &viewMatrix)
{
    struct Entry { RenderItem item; float depth; };
    std::vector<Entry> sorted;
    sorted.reserve(items.size());
    for (const auto &item : items)
    {
        if (item.transform == nullptr)
        {
            throw std::invalid_argument("Transparent item requires a transform with finite view depth");
        }
        // 每项只计算一次排序键，比较器不再反复遍历父链；下次调用重新计算，不跨帧缓存。
        sorted.push_back({item, transparentViewDepth(item.transform->worldMatrix(), item.sortOrigin, viewMatrix)});
    }
    std::stable_sort(sorted.begin(), sorted.end(),
        [](const Entry &left, const Entry &right)
        {
            // 摄像机面向视空间-Z方向，因此越远的物体Z越小。
            return left.depth < right.depth;
        });
    for (std::size_t index = 0; index < items.size(); ++index) { items[index] = sorted[index].item; }
}
