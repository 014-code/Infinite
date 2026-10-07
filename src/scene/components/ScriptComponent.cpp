#include "scene/components/ScriptComponent.h"

#include "scene/GameObject.h"

#include <utility>

void ScriptComponent::setUpdateCallback(UpdateCallback callback)
{
    updateCallback_ = callback ? std::make_shared<UpdateCallback>(std::move(callback)) : nullptr;
}

bool ScriptComponent::hasUpdateCallback() const noexcept
{
    return updateCallback_ != nullptr;
}

void ScriptComponent::update(GameObject &object, float deltaTime) const
{
    if (!updateCallback_)
    {
        return;
    }

    // 不能直接解引用成员后执行：回调内部可能通过setUpdateCallback替换自己，
    // 替换会释放旧shared_ptr。局部副本能保证旧函数对象活到本次调用结束。
    const std::shared_ptr<UpdateCallback> callback = updateCallback_;
    (*callback)(object, deltaTime);
}
