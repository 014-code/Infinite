#include "resources/ResourceManager.h"
#include "save/SaveGame.h"
#include "scene/Scene.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char *message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    SaveGameData makeData(const std::string &sceneName, std::int64_t score)
    {
        SaveGameData data;
        data.activeScene = sceneName;
        data.values["score"] = score;
        return data;
    }
}

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "无限引擎存档测试";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    try
    {
        ResourceManager resources;
        Scene scene;
        scene.createObject("SavedObject");
        SaveGameData data = makeData("level_01", 42);
        data.values["ratio"] = 0.75;
        data.values["paused"] = false;
        data.values["player_name"] = std::string("Player");
        SaveGameSerializer::save(scene, data, directory);

        const SaveGameData initial = SaveGameSerializer::load(scene, directory, resources);
        require(initial.activeScene == "level_01" && scene.objectCount() == 1,
            "Initial save did not restore scene and active level");
        require(std::get<std::string>(initial.values.at("player_name")) == "Player",
            "Initial string save value was corrupted");

        // 覆盖已有存档后，新版本应成为当前版本，且不应留下固定名称的.tmp目录。
        scene.createObject("SecondObject");
        SaveGameData replacement = makeData("level_02", 84);
        SaveGameSerializer::save(scene, replacement, directory);
        const SaveGameData replaced = SaveGameSerializer::load(scene, directory, resources);
        require(replaced.activeScene == "level_02" && scene.objectCount() == 2,
            "Save replacement did not become the current version");
        require(std::get<std::int64_t>(replaced.values.at("score")) == 84,
            "Replacement save value was corrupted");

        // 写入阶段失败时，正式目录尚未切换，旧存档必须仍然可读。
        SaveGameData invalid = replacement;
        invalid.version = 999;
        bool failed = false;
        try { SaveGameSerializer::save(scene, invalid, directory); }
        catch (const std::invalid_argument &) { failed = true; }
        require(failed, "Invalid save data did not fail before commit");
        const SaveGameData afterFailure = SaveGameSerializer::load(scene, directory, resources);
        require(afterFailure.activeScene == "level_02" &&
                std::get<std::int64_t>(afterFailure.values.at("score")) == 84,
            "Failed save damaged the previous save");

        scene.clear();
        const SaveGameData loaded = SaveGameSerializer::load(scene, directory, resources);
        require(loaded.activeScene == "level_02" && scene.objectCount() == 2,
            "Save did not restore scene and active level");
        require(std::get<std::int64_t>(loaded.values.at("score")) == 84,
            "Integer save value was corrupted");
        std::filesystem::remove_all(directory, error);
        std::cout << "Save game passed\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::filesystem::remove_all(directory, error);
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
