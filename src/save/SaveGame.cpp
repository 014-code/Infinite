#include "SaveGame.h"

#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "resources/ResourceManager.h"

#include <fstream>
#include <cmath>
#include <chrono>
#include <exception>
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

    // 为一次保存生成不会覆盖其他临时目录的同级路径。
    // 临时目录和目标目录放在同一个父目录，文件系统才能在同一卷内完成快速改名。
    std::filesystem::path makeUniqueSiblingPath(
        const std::filesystem::path &directory, const char *suffix)
    {
        const auto parent = directory.parent_path().empty()
            ? std::filesystem::current_path()
            : directory.parent_path();
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

        for (int attempt = 0; attempt < 100; ++attempt)
        {
            std::filesystem::path candidate = parent / directory.filename();
            candidate += std::string(suffix) + "-" + std::to_string(timestamp)
                + "-" + std::to_string(attempt);

            std::error_code error;
            if (std::filesystem::exists(candidate, error))
            {
                continue;
            }
            if (error)
            {
                throw std::runtime_error(
                    "Cannot inspect save sibling path: " + error.message());
            }
            return candidate;
        }

        throw std::runtime_error("Cannot allocate a unique save sibling path");
    }

    // 清理失败不能覆盖保存流程中更重要的异常，因此这里只做尽力清理。
    void removeBestEffort(const std::filesystem::path &path) noexcept
    {
        if (path.empty())
        {
            return;
        }
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

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
    // 先写入同级唯一临时目录，避免半写入的state.save或scene.scene被当成正式存档读取。
    const auto temporary = makeUniqueSiblingPath(directory, ".tmp");
    // 旧目录会短暂保存在备份路径；只有新目录成功接管目标路径后才清理它。
    const auto backup = makeUniqueSiblingPath(directory, ".backup");
    std::error_code error;

    bool oldSaveMoved = false;
    try
    {
        std::filesystem::create_directories(temporary, error);
        if (error) { throw std::runtime_error("Cannot create temporary save directory: " + error.message()); }
        SceneSerializer::save(scene, temporary / "scene.scene");
        writeState(data, temporary / "state.save");

        // 不先删除旧目录。先改名保留旧版本，提交失败时才能恢复它。
        const bool oldSaveExists = std::filesystem::exists(directory, error);
        if (error)
        {
            throw std::runtime_error("Cannot inspect old save: " + error.message());
        }
        if (oldSaveExists)
        {
            std::filesystem::rename(directory, backup, error);
            if (error)
            {
                throw std::runtime_error("Cannot move old save to backup: " + error.message());
            }
            oldSaveMoved = true;
        }

        std::filesystem::rename(temporary, directory, error);
        if (error)
        {
            throw std::runtime_error("Cannot commit save directory: " + error.message());
        }

        // 新存档已经成为正式版本。备份只用于提交失败恢复，删除失败不应让一次成功保存报错。
        if (oldSaveMoved)
        {
            removeBestEffort(backup);
        }
    }
    catch (...)
    {
        const std::exception_ptr failure = std::current_exception();
        removeBestEffort(temporary);

        if (oldSaveMoved)
        {
            // 正常情况下目标目录尚不存在；若切换过程留下了半成品，先删除它再恢复旧版本。
            removeBestEffort(directory);
            std::error_code restoreError;
            std::filesystem::rename(backup, directory, restoreError);
            if (restoreError)
            {
                throw std::runtime_error(
                    "Save failed and old save could not be restored: "
                    + restoreError.message());
            }
        }

        std::rethrow_exception(failure);
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
