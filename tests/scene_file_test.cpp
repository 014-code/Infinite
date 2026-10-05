#include "TestSupport.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "scene/serialization/PendingSceneFile.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
    std::string readText(const std::filesystem::path &path)
    {
        std::ifstream input(path, std::ios::binary);
        require(input.good(), "Cannot read saved test scene");
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    std::set<std::filesystem::path> entries(const std::filesystem::path &directory)
    {
        std::set<std::filesystem::path> result;
        for (const auto &entry : std::filesystem::directory_iterator(directory)) { result.insert(entry.path()); }
        return result;
    }

    void verifyInterruptedWrite(const std::filesystem::path &path)
    {
        const auto before = entries(path.parent_path());
        const auto original = readText(path);
        // 在提交前人为中断，验证实际的临时文件写入及RAII回收，而不是只验证保存前校验。
        // 这不模拟磁盘写满；磁盘故障由write/flush/sync/close的返回值检查处理。
        expectThrow<std::runtime_error>([&]
        {
            scene_serialization::PendingSceneFile pending(path);
            pending.write("partial data that must never replace the old scene");
            require(readText(path) == original, "Uncommitted data was published");
            throw std::runtime_error("Injected interruption before commit");
        }, "Interrupted transaction did not throw");
        require(readText(path) == original, "Interrupted save damaged the previous file");
        require(entries(path.parent_path()) == before, "Interrupted save left a temporary file");
    }
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected scene file test directory");
        const auto directory = std::filesystem::absolute(argv[1]) / std::filesystem::u8path("中文存档");
        std::filesystem::create_directories(directory);
        const auto path = directory / std::filesystem::u8path("场景.scene");
        Scene scene;
        auto &object = scene.createObject("old object");
        SceneSerializer::save(scene, path);
        const auto original = readText(path);
        [[maybe_unused]] const auto before = entries(directory);

        object.transform.position.x = std::numeric_limits<float>::infinity();
        expectThrow<std::runtime_error>([&] { SceneSerializer::save(scene, path); }, "Invalid transform saved");
        require(readText(path) == original, "Validation error damaged the previous save");
        object.transform.position.x = 0.0f;
        verifyInterruptedWrite(path);

#ifdef _WIN32
        // Windows只共享读取，不共享删除/替换；提交必须报错，旧档字节不能有变化。
        // RAII句柄确保即使测试断言失败也会解除锁定。
        struct ReadLock
        {
            HANDLE handle;
            ~ReadLock() { if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); } }
        };
        {
            ReadLock lock{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
            require(lock.handle != INVALID_HANDLE_VALUE, "Could not lock the test scene");
            object.setName("replacement");
            expectThrow<std::runtime_error>([&] { SceneSerializer::save(scene, path); }, "Locked target was replaced");
            require(readText(path) == original, "Failed replacement damaged the old save");
        }
        require(entries(directory) == before, "Failed replacement left a temporary file");
#endif

        // 目录不能被文件替换。此失败路径在Windows和POSIX都执行，且不能留下tmp。
        const auto blocked = directory / "directory.scene";
        std::filesystem::create_directory(blocked);
        const auto withDirectory = entries(directory);
        expectThrow<std::runtime_error>([&] { SceneSerializer::save(scene, blocked); }, "Directory was replaced");
        require(std::filesystem::is_directory(blocked), "Failed save removed the destination directory");
        require(entries(directory) == withDirectory, "Directory replacement failure leaked a temporary file");

        // 父路径为普通文件，临时文件创建即失败，也不能影响这个父文件。
        expectThrow<std::runtime_error>([&] { SceneSerializer::save(scene, path / "child.scene"); },
            "File was accepted as a parent directory");
        require(readText(path) == original, "Creation failure changed the parent file");

        object.setName("replacement");
        SceneSerializer::save(scene, path);
        require(readText(path) != original, "Successful save did not replace the old content");
        Scene restored;
        const auto ids = SceneSerializer::load(restored, path);
        require(ids.size() == 1 && restored.findObject(ids[0])->name() == "replacement", "Replacement cannot be loaded");
        require(entries(directory) == withDirectory, "Successful commit left a temporary file");

        // 连续提交两个临时文件：独占创建不能互相覆盖，也不能清理另一个事务的临时文件。
        {
            scene_serialization::PendingSceneFile first(path), second(path);
            first.write("first");
            second.write("second");
            first.commit();
            require(readText(path) == "first", "First transaction was overwritten before second commit");
            second.commit();
            require(readText(path) == "second", "Second transaction did not publish its own data");
        }
        SceneSerializer::save(scene, path); // 保留可读的测试产物，方便检查。
        std::cout << "Scene file safety passed: replace, interruption, failure cleanup and Unicode paths\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Scene file safety failed: " << error.what() << '\n';
        return 1;
    }
}
