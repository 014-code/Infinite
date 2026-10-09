#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

class ResourceManager;
class Scene;

// SceneManager第一阶段只负责同步关卡切换。
// Loader先在临时Scene中创建对象，全部成功后才提交，失败时当前关卡保持不变。
class SceneManager final
{
public:
    using SceneLoader = std::function<void(Scene &, ResourceManager &)>;

    SceneManager(Scene &scene, ResourceManager &resources)
        : scene_(&scene), resources_(&resources)
    {
    }

    SceneManager(const SceneManager &) = delete;
    SceneManager &operator=(const SceneManager &) = delete;

    void registerScene(std::string name, SceneLoader loader);
    bool hasScene(const std::string &name) const noexcept;

    // 立即切换。推荐在Application事件阶段或状态切换提交阶段调用。
    void load(const std::string &name);
    void unload() noexcept;

    // 请求下一帧切换；Application会在Scene更新前提交，避免更新遍历中清空Scene。
    void requestLoad(std::string name);
    bool hasPendingLoad() const noexcept { return pendingName_.has_value(); }
    bool commitPending();

    const std::string &currentName() const noexcept { return currentName_; }

private:
    Scene *scene_ = nullptr;
    ResourceManager *resources_ = nullptr;
    std::unordered_map<std::string, SceneLoader> loaders_;
    std::string currentName_;
    std::optional<std::string> pendingName_;
};
