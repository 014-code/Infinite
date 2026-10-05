#pragma once

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <vector>

class Transform
{
public:
    // 当Transform没有父节点时，这些值就是世界空间的变换；有父节点时，
    // 它们表示相对于父节点的局部变换。保留公开成员是为了兼容当前示例代码。
    glm::vec3 position{0.0f};

    // 物体在三个坐标轴上的缩放比例
    glm::vec3 scale{1.0f};

    Transform() = default;
    ~Transform();

    // 以弧度设置欧拉角。欧拉角接口主要用于编辑器、场景文件兼容和简单示例；
    // Transform内部始终保存四元数，避免公开两个可能互相不同步的旋转状态。
    void setEulerAngles(const glm::vec3 &angles);
    glm::vec3 eulerAngles() const;

    // 获取或设置旋转四元数；设置时自动归一化，拒绝全零以及包含NaN/无穷大的输入。
    // GLM构造参数顺序为(w, x, y, z)，glm::quat(1, 0, 0, 0)表示不旋转。
    const glm::quat &rotation() const;
    void setRotation(const glm::quat &rotation);

    // 右乘增量，在物体局部坐标轴上旋转；不会执行欧拉角分量相加。
    // 欧拉角转换约定是Rx * Ry * Rz，列向量实际先受Z旋转，再Y，最后X。
    void rotate(const glm::quat &delta);
    void rotateEuler(const glm::vec3 &deltaAngles);

    // 父节点只借用，不负责销毁；父节点析构会自动解除子节点链接。
    // 设置父节点后，当前position、rotation和scale仍保持原值，因此世界变换可能立即改变。
    void setParent(Transform *parent);
    Transform *parent();
    const Transform *parent() const;

    // 返回直接子节点列表。列表由Transform内部维护，调用者只能读取，不能保存其中指针跨越
    // 子节点析构或重新设置父节点的操作继续使用。
    const std::vector<Transform *> &children() const;

    // 根据局部位置、旋转和缩放生成只考虑自身的矩阵。
    glm::mat4 localMatrix() const;

    // 将局部矩阵乘到父节点世界矩阵上，得到最终用于渲染的世界矩阵。
    glm::mat4 worldMatrix() const;

    // 旧接口保留为世界矩阵别名，避免Renderer和现有示例因为层级功能改动而分散修改。
    glm::mat4 modelMatrix() const;

    // 复制只复制局部位置、旋转四元数和缩放，不复制父子链接；否则新对象会错误地登记到
    // 原对象的父节点。复制赋值也保留目标对象原有的层级关系，只替换局部变换值。
    Transform(const Transform &other);
    Transform &operator=(const Transform &other);

    // 父子链接包含反向指针，移动会让父节点列表继续指向旧地址，因此禁止移动。
    Transform(Transform &&) = delete;
    Transform &operator=(Transform &&) = delete;

private:
    // 只维护这一份旋转状态，欧拉角通过接口换算，避免两份数据互相不同步。
    glm::quat rotation_{1.0f, 0.0f, 0.0f, 0.0f};
    Transform *parent_ = nullptr;
    std::vector<Transform *> children_;
};
