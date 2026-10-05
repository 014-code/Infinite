#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vector>

class Mesh;
class Material;
class Transform;

struct RenderItem
{
    // 一次绘制需要的借用数据，不持有资源所有权。消费列表前不得销毁或移动资源。
    const Mesh *mesh = nullptr;
    const Material *material = nullptr;
    const Transform *transform = nullptr;
    glm::vec3 sortOrigin{0.0f};
    // 本帧快照；空数组表示不蒙皮。由Scene解析关节ID后填入，Renderer不依赖Scene。
    std::vector<glm::mat4> skinMatrices{};
};

// 已有世界矩阵时直接复用，不再回溯Transform父链；遇到非有限深度在排序前抛异常。
float transparentViewDepth(const glm::mat4 &worldMatrix, const glm::vec3 &sortOrigin,
    const glm::mat4 &viewMatrix);

// 根据view * model * sortOrigin的Z值，从远到近稳定排序；等深度保持提交顺序。
// 此函数不使用OpenGL，方便单独验证不同摄像机方向下的排序。
// 参考点排序适合互不穿插的物体，不解决相交透明网格的逐三角形可见性。
void sortTransparentBackToFront(std::vector<RenderItem> &items, const glm::mat4 &viewMatrix);
