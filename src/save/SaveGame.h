#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <variant>

class ResourceManager;
class Scene;

using SaveValue = std::variant<std::int64_t, double, bool, std::string>;

// SaveGameData保存跨关卡的应用状态；Scene本身仍由SceneSerializer保存。
// 用variant而不是void*，让存档类型在读取时可校验、可迁移。
struct SaveGameData
{
    std::uint32_t version = 1;
    std::string activeScene;
    std::map<std::string, SaveValue> values;
};

// 完整存档目录：state.save保存游戏数据，scene.scene保存当前Scene结构。
// 写入采用临时目录完成后替换，读取时先解析元数据再让SceneSerializer原子加载场景。
class SaveGameSerializer final
{
public:
    static void save(const Scene &scene, const SaveGameData &data,
        const std::filesystem::path &directory);
    static SaveGameData load(Scene &scene, const std::filesystem::path &directory,
        ResourceManager &resources);
};
