#include "SceneManager.h"

#include "Scene.h"

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

    // 先在独立Scene中完整加载。Loader失败时，正式Scene和currentName_都不改变。
    Scene staging;
    scene_->prepareStaging(staging);
    iterator->second(staging, *resources_);

    // 先准备好名称，再用swap完成不抛异常的提交，避免名称分配失败造成“场景已换但名称仍旧”的状态。
    std::string committedName = name;
    // 交换的是unique_ptr和索引，不会移动GameObject地址；staging析构时只销毁旧关卡。
    scene_->replaceContents(staging);
    currentName_.swap(committedName);
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
    try
    {
        load(name);
    }
    catch (...)
    {
        // 失败时保留请求，调用方修复资源后可以再次提交；当前场景仍由load的staging保证不变。
        pendingName_ = std::move(name);
        throw;
    }
    return true;
}
