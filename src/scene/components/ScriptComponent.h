#pragma once

#include "scene/components/Component.h"

#include <functional>
#include <memory>

class GameObject;

// ScriptComponent保存GameObject的应用层更新逻辑。
//
// 它不负责输入、物理或渲染，只提供一个稳定的每帧逻辑入口。具体游戏代码可以
// 通过捕获外部状态实现移动、旋转和玩法规则，而Scene只负责按固定顺序调度组件。
class ScriptComponent final : public Component
{
public:
    ScriptComponent() = default;
    explicit ScriptComponent(GameObject &owner) noexcept : Component(owner) {}
    ~ScriptComponent() override = default;

    using UpdateCallback = std::function<void(GameObject &, float)>;

    // 空回调表示当前没有脚本逻辑；重复设置会替换旧回调。
    // 回调在Scene更新期间执行，不能在回调中增删Scene对象或递归调用Scene::update。
    void setUpdateCallback(UpdateCallback callback);

    bool hasUpdateCallback() const noexcept;

    // Scene使用这个入口执行逻辑。执行前会临时复制shared_ptr，
    // 因此回调可以在运行中替换或清空自己而不会提前销毁正在执行的函数对象。
    void update(GameObject &object, float deltaTime) const;

private:
    // shared_ptr的作用不是共享给外部，而是让“当前正在执行的回调”和组件共同持有
    // 同一个函数对象，支持mutable lambda跨帧保存状态以及回调自替换。
    std::shared_ptr<UpdateCallback> updateCallback_;
};
