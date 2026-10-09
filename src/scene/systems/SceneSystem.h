#pragma once

class Scene;

// SceneSystem是场景级扩展点，不拥有Scene，也不拥有场景中的GameObject。
// 适合把跨多个对象的规则封装起来，例如AI、任务目标或游戏层统计。
// 当前系统在Scene::update中执行；物理固定步和渲染同步仍由引擎内部阶段负责。
class SceneSystem
{
public:
    virtual ~SceneSystem() = default;

    SceneSystem(const SceneSystem &) = delete;
    SceneSystem &operator=(const SceneSystem &) = delete;

    // 系统对象必须在注册期间保持地址稳定，并在Scene析构或清理前主动注销。
    // 回调内不能创建/删除Scene对象，也不能递归调用Scene::update。
    virtual void update(Scene &scene, float deltaTime) = 0;

protected:
    SceneSystem() = default;
};
