#include "Transform.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    glm::quat quaternionFromEulerAngles(const glm::vec3 &angles)
    {
        // 保持旧版矩阵Rx * Ry * Rz。对列向量实际先作用Z，再Y，最后X。
        return glm::angleAxis(angles.x, glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::angleAxis(angles.y, glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::angleAxis(angles.z, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    void requireFinite(const glm::quat &rotation)
    {
        if (!std::isfinite(rotation.w) || !std::isfinite(rotation.x) ||
            !std::isfinite(rotation.y) || !std::isfinite(rotation.z))
        {
            throw std::invalid_argument("Transform rotation must be finite");
        }
        if (rotation.w == 0 && rotation.x == 0 && rotation.y == 0 && rotation.z == 0)
        {
            throw std::invalid_argument("Transform rotation must not be zero");
        }
    }
}

Transform::Transform(const Transform &other)
    : position(other.position), scale(other.scale), rotation_(other.rotation_)
{
    // 复制出的Transform默认是独立根节点，不继承原对象的父子关系。
}

Transform &Transform::operator=(const Transform &other)
{
    if (this != &other)
    {
        // 赋值只替换局部变换，保留当前对象已经建立的父子链接。
        position = other.position;
        scale = other.scale;
        rotation_ = other.rotation_;
        // 赋值覆盖了局部变换，即使数值碰巧相同，也显式让缓存重新验证父节点关系。
        localCacheValid_ = false;
        worldCacheValid_ = false;
    }
    return *this;
}

void Transform::setEulerAngles(const glm::vec3 &angles)
{
    if (!std::isfinite(angles.x) || !std::isfinite(angles.y) || !std::isfinite(angles.z))
    {
        throw std::invalid_argument("Transform Euler angles must be finite");
    }
    setRotation(quaternionFromEulerAngles(angles));
}

glm::vec3 Transform::eulerAngles() const
{
    // 从四元数对应的旋转矩阵恢复与旧版Rx * Ry * Rz一致的欧拉角。
    // 这个接口主要用于编辑器和兼容显示；真正的变换计算始终直接使用四元数。
    const glm::mat4 matrix = glm::mat4_cast(rotation_);
    const float sineOfY = std::clamp(matrix[2][0], -1.0f, 1.0f);
    const float y = std::asin(sineOfY);
    const float cosineOfY = std::cos(y);
    if (std::abs(cosineOfY) > 0.00001f)
    {
        return {
            std::atan2(-matrix[2][1], matrix[2][2]),
            y,
            std::atan2(-matrix[1][0], matrix[0][0])
        };
    }

    // 万向节锁时X/Z无法唯一确定。固定X为0，返回一组稳定结果即可；
    // 内部四元数仍然完整保留，不会因为读取欧拉角而丢失旋转信息。
    return {0.0f, y, std::atan2(matrix[0][1], matrix[1][1])};
}

const glm::quat &Transform::rotation() const
{
    return rotation_;
}

void Transform::setRotation(const glm::quat &rotation)
{
    requireFinite(rotation);
    // 先按最大分量缩放，防止很大/很小但有限的输入在长度计算时溢出/下溢。
    const float largest = std::max({std::abs(rotation.w), std::abs(rotation.x),
        std::abs(rotation.y), std::abs(rotation.z)});
    rotation_ = glm::normalize(rotation / largest);
}

void Transform::rotate(const glm::quat &delta)
{
    requireFinite(delta);
    Transform normalizedDelta;
    normalizedDelta.setRotation(delta);
    setRotation(rotation_ * normalizedDelta.rotation());
}

void Transform::rotateEuler(const glm::vec3 &deltaAngles)
{
    if (!std::isfinite(deltaAngles.x) || !std::isfinite(deltaAngles.y) ||
        !std::isfinite(deltaAngles.z))
    {
        throw std::invalid_argument("Transform Euler rotation delta must be finite");
    }
    rotate(quaternionFromEulerAngles(deltaAngles));
}

Transform::~Transform()
{
    // 子节点不是Transform的所有者。父节点销毁时，子节点自动变成根节点，
    // 这样它们仍然可以安全地继续使用自己的局部变换。
    if (parent_ != nullptr)
    {
        auto &siblings = parent_->children_;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }

    // 当前节点销毁后，子节点不能继续保存指向它的悬空父指针。
    for (Transform *child : children_)
    {
        child->parent_ = nullptr;
        child->worldCacheValid_ = false;
    }
}

void Transform::setParent(Transform *parent)
{
    if (parent == this)
    {
        throw std::invalid_argument("Transform cannot be its own parent");
    }
    if (parent == parent_)
    {
        return;
    }

    // 从候选父节点一路向上检查，防止把祖先设置成自己的子节点。
    // 例如 A -> B 时，不能再把A设置为B的子节点，否则worldMatrix会无限递归。
    for (Transform *ancestor = parent; ancestor != nullptr; ancestor = ancestor->parent_)
    {
        if (ancestor == this)
        {
            throw std::invalid_argument("Transform parent relationship would create a cycle");
        }
    }

    // 先加入新父节点的列表。vector扩容可能抛异常，先做这一步可以保证异常发生时
    // 旧父子关系仍然完整；新关系成功登记后再解除旧父节点的反向链接。
    if (parent != nullptr)
    {
        parent->children_.push_back(this);
    }
    if (parent_ != nullptr)
    {
        auto &siblings = parent_->children_;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }
    parent_ = parent;
    // worldMatrix会再次检查父节点指针和版本；显式失效让意图更清楚，
    // 也避免父节点恰好使用相同版本号时误用旧缓存。
    worldCacheValid_ = false;
}

Transform *Transform::parent()
{
    return parent_;
}

const Transform *Transform::parent() const
{
    return parent_;
}

const std::vector<Transform *> &Transform::children() const
{
    return children_;
}

glm::mat4 Transform::localMatrix() const
{
    refreshLocalCache();
    return localCache_;
}

glm::mat4 Transform::worldMatrix() const
{
    refreshLocalCache();
    if (parent_ == nullptr)
    {
        if (!worldCacheValid_ || cachedParent_ != nullptr ||
            cachedWorldLocalRevision_ != localRevision_)
        {
            worldCache_ = localCache_;
            cachedParent_ = nullptr;
            cachedParentWorldRevision_ = 0;
            cachedWorldLocalRevision_ = localRevision_;
            worldCacheValid_ = true;
            bumpRevision();
        }
        return worldCache_;
    }

    // 先让父节点更新自己的缓存，再读取它的版本号；这样无论是父节点自身还是更高祖先
    // 被直接修改，当前节点都能看到新的parent world revision。
    const glm::mat4 parentWorld = parent_->worldMatrix();
    if (!worldCacheValid_ || cachedParent_ != parent_ ||
        cachedParentWorldRevision_ != parent_->worldRevision_ ||
        cachedWorldLocalRevision_ != localRevision_)
    {
        worldCache_ = parentWorld * localCache_;
        cachedParent_ = parent_;
        cachedParentWorldRevision_ = parent_->worldRevision_;
        cachedWorldLocalRevision_ = localRevision_;
        worldCacheValid_ = true;
        bumpRevision();
    }
    return worldCache_;
}

void Transform::refreshLocalCache() const
{
    if (localCacheValid_ && cachedPosition_ == position && cachedScale_ == scale &&
        cachedRotation_ == rotation_)
    {
        return;
    }

    // GLM按列向量约定组合矩阵；依次调用translate、rotate、scale后，
    // 顶点会先缩放，再旋转，最后平移，符合常用的局部变换语义。
    glm::mat4 local(1.0f);
    local = glm::translate(local, position);
    local *= glm::mat4_cast(rotation_);
    local = glm::scale(local, scale);
    localCache_ = local;
    cachedPosition_ = position;
    cachedScale_ = scale;
    cachedRotation_ = rotation_;
    localCacheValid_ = true;
    ++localRevision_;
    if (localRevision_ == 0) { localRevision_ = 1; }
    worldCacheValid_ = false;
}

void Transform::bumpRevision() const noexcept
{
    ++worldRevision_;
    if (worldRevision_ == 0) { worldRevision_ = 1; }
}

glm::mat4 Transform::modelMatrix() const
{
    return worldMatrix();
}
