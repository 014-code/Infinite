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
}

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "infinite_save_game_test";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    try
    {
        ResourceManager resources;
        Scene scene;
        scene.createObject("SavedObject");
        SaveGameData data;
        data.activeScene = "level_01";
        data.values["score"] = std::int64_t(42);
        data.values["ratio"] = 0.75;
        data.values["paused"] = false;
        data.values["player_name"] = std::string("Player");
        SaveGameSerializer::save(scene, data, directory);

        scene.clear();
        const SaveGameData loaded = SaveGameSerializer::load(scene, directory, resources);
        require(loaded.activeScene == "level_01" && scene.objectCount() == 1,
            "Save did not restore scene and active level");
        require(std::get<std::int64_t>(loaded.values.at("score")) == 42,
            "Integer save value was corrupted");
        require(std::get<std::string>(loaded.values.at("player_name")) == "Player",
            "String save value was corrupted");
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
