#include "SceneManager.h"

#include "Scene.h"

#include <optional>
#include <stdexcept>
#include <utility>

void SceneManager::registerScene(std::string name, SceneLoader loader)
{
    if (name.empty())
    {
        throw std::invalid_argument("Scene name must not be empty");
    }
    if (!loader)
    {
        throw std::invalid_argument("Scene loader must not be empty");
    }
    if (!loaders_.emplace(name, std::move(loader)).second)
    {
        throw std::invalid_argument("Scene name is already registered: " + name);
    }
}

bool SceneManager::hasScene(const std::string &name) const noexcept
{
    return loaders_.find(name) != loaders_.end();
}

void SceneManager::load(const std::string &name)
{
    const auto iterator = loaders_.find(name);
    if (iterator == loaders_.end())
    {
        throw std::out_of_range("Scene is not registered: " + name);
    }

    // 先清理当前关卡，再调用新Loader；失败时也清理Loader已经创建的半成品。
    // 这样不会留下“当前名称是A、对象却来自B一半”的混合状态。
    scene_->clear();
    currentName_.clear();
    try
    {
        iterator->second(*scene_, *resources_);
        currentName_ = name;
    }
    catch (...)
    {
        scene_->clear();
        throw;
    }
}

void SceneManager::unload() noexcept
{
    pendingName_.reset();
    scene_->clear();
    currentName_.clear();
}

void SceneManager::requestLoad(std::string name)
{
    if (name.empty())
    {
        throw std::invalid_argument("Pending scene name must not be empty");
    }
    if (!hasScene(name))
    {
        throw std::out_of_range("Scene is not registered: " + name);
    }
    pendingName_ = std::move(name);
}

bool SceneManager::commitPending()
{
    if (!pendingName_)
    {
        return false;
    }
    std::string name = std::move(*pendingName_);
    pendingName_.reset();
    load(name);
    return true;
}
