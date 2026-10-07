#include "SaveGame.h"

#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "resources/ResourceManager.h"

#include <fstream>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace
{
    constexpr std::uint32_t kCurrentVersion = 1;
    constexpr std::size_t kMaximumEntries = 100000;
    constexpr std::size_t kMaximumStringLength = 1024 * 1024;

    void requireString(const std::string &value, const char *name)
    {
        if (value.size() > kMaximumStringLength)
        {
            throw std::runtime_error(std::string(name) + " is too long");
        }
    }

    void writeValue(std::ostream &stream, const SaveValue &value)
    {
        if (std::holds_alternative<std::int64_t>(value))
        {
            stream << "I " << std::get<std::int64_t>(value);
        }
        else if (std::holds_alternative<double>(value))
        {
            stream << "D " << std::setprecision(17) << std::get<double>(value);
        }
        else if (std::holds_alternative<bool>(value))
        {
            stream << "B " << (std::get<bool>(value) ? 1 : 0);
        }
        else
        {
            stream << "S " << std::quoted(std::get<std::string>(value));
        }
    }

    SaveValue readValue(std::istream &stream)
    {
        char type = 0;
        if (!(stream >> type)) { throw std::runtime_error("Malformed save value type"); }
        if (type == 'I')
        {
            std::int64_t value = 0;
            if (!(stream >> value)) { throw std::runtime_error("Malformed integer save value"); }
            return value;
        }
        if (type == 'D')
        {
            double value = 0.0;
            if (!(stream >> value) || !std::isfinite(value))
            { throw std::runtime_error("Malformed floating save value"); }
            return value;
        }
        if (type == 'B')
        {
            int value = 0;
            if (!(stream >> value) || (value != 0 && value != 1))
            { throw std::runtime_error("Malformed boolean save value"); }
            return value != 0;
        }
        if (type == 'S')
        {
            std::string value;
            if (!(stream >> std::quoted(value))) { throw std::runtime_error("Malformed string save value"); }
            requireString(value, "Save string");
            return value;
        }
        throw std::runtime_error("Unknown save value type");
    }

    void writeState(const SaveGameData &data, const std::filesystem::path &path)
    {
        if (data.version != kCurrentVersion)
        {
            throw std::invalid_argument("Unsupported save version");
        }
        requireString(data.activeScene, "Active scene");
        if (data.values.size() > kMaximumEntries)
        {
            throw std::invalid_argument("Too many save values");
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream) { throw std::runtime_error("Cannot open save state for writing"); }
        stream << "INFINITE_SAVE " << data.version << '\n';
        stream << "SCENE " << std::quoted(data.activeScene) << '\n';
        for (const auto &[key, value] : data.values)
        {
            requireString(key, "Save key");
            stream << "VALUE " << std::quoted(key) << ' ';
            writeValue(stream, value);
            stream << '\n';
        }
        stream.flush();
        if (!stream) { throw std::runtime_error("Failed to write save state"); }
    }

    SaveGameData readState(const std::filesystem::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) { throw std::runtime_error("Cannot open save state"); }
        std::string header;
        SaveGameData data;
        if (!(stream >> header >> data.version) || header != "INFINITE_SAVE" ||
            data.version != kCurrentVersion)
        {
            throw std::runtime_error("Unsupported or malformed save version");
        }
        std::string token;
        if (!(stream >> token) || token != "SCENE" || !(stream >> std::quoted(data.activeScene)))
        {
            throw std::runtime_error("Malformed save scene field");
        }
        requireString(data.activeScene, "Active scene");
        while (stream >> token)
        {
            if (token != "VALUE" || data.values.size() >= kMaximumEntries)
            {
                throw std::runtime_error("Malformed or oversized save value section");
            }
            std::string key;
            if (!(stream >> std::quoted(key))) { throw std::runtime_error("Malformed save key"); }
            requireString(key, "Save key");
            if (!data.values.emplace(key, readValue(stream)).second)
            {
                throw std::runtime_error("Duplicate save key: " + key);
            }
        }
        if (!stream.eof()) { throw std::runtime_error("Failed while reading save state"); }
        return data;
    }
}

void SaveGameSerializer::save(const Scene &scene, const SaveGameData &data,
    const std::filesystem::path &directory)
{
    if (directory.empty() || directory.filename().empty())
    {
        throw std::invalid_argument("Save directory must not be empty");
    }
    const auto parent = directory.parent_path().empty() ? std::filesystem::current_path() : directory.parent_path();
    const auto temporary = parent / (directory.filename().u8string() + ".tmp");
    std::error_code error;
    std::filesystem::remove_all(temporary, error);
    if (error) { throw std::runtime_error("Cannot clear temporary save directory: " + error.message()); }
    std::filesystem::create_directories(temporary, error);
    if (error) { throw std::runtime_error("Cannot create temporary save directory: " + error.message()); }
    try
    {
        SceneSerializer::save(scene, temporary / "scene.scene");
        writeState(data, temporary / "state.save");
        std::filesystem::remove_all(directory, error);
        if (error) { throw std::runtime_error("Cannot replace old save: " + error.message()); }
        std::filesystem::rename(temporary, directory, error);
        if (error) { throw std::runtime_error("Cannot commit save directory: " + error.message()); }
    }
    catch (...)
    {
        std::filesystem::remove_all(temporary, error);
        throw;
    }
}

SaveGameData SaveGameSerializer::load(Scene &scene, const std::filesystem::path &directory,
    ResourceManager &resources)
{
    if (!std::filesystem::is_directory(directory))
    {
        throw std::runtime_error("Save directory does not exist: " + directory.u8string());
    }
    SaveGameData data = readState(directory / "state.save");
    SceneSerializer::load(scene, directory / "scene.scene", resources);
    return data;
}
