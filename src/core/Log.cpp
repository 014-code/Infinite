#include "Log.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <utility>

namespace
{
    struct LogState
    {
        std::mutex mutex;
        Log::Level minimumLevel = Log::Level::Info;
        bool consoleEnabled = true;
        std::ofstream file;
    };

    LogState &state()
    {
        // 第一次使用时创建，避免依赖不同.cpp文件中全局变量的初始化顺序。
        static LogState value;
        return value;
    }

    bool accepts(const LogState &value, Log::Level level)
    {
        return level != Log::Level::Off && level >= value.minimumLevel;
    }

    const char *levelName(Log::Level level)
    {
        switch (level)
        {
        case Log::Level::Debug: return "DEBUG";
        case Log::Level::Info: return "INFO";
        case Log::Level::Warning: return "WARN";
        case Log::Level::Error: return "ERROR";
        default: return "OFF";
        }
    }

    std::string_view fileName(const char *file)
    {
        // 同时识别Windows和Unix路径，控制台只显示文件名，避免长路径淹没信息。
        const std::string_view path = file == nullptr ? "unknown" : file;
        const auto separator = path.find_last_of("/\\");
        return separator == std::string_view::npos ? path : path.substr(separator + 1);
    }
}

void Log::setLevel(Level level)
{
    auto &value = state();
    const std::lock_guard<std::mutex> lock(value.mutex);
    value.minimumLevel = level;
}

bool Log::isEnabled(Level level)
{
    auto &value = state();
    const std::lock_guard<std::mutex> lock(value.mutex);
    return accepts(value, level);
}

void Log::setConsoleEnabled(bool enabled)
{
    auto &value = state();
    const std::lock_guard<std::mutex> lock(value.mutex);
    value.consoleEnabled = enabled;
}

bool Log::setFile(const std::filesystem::path &path) noexcept
{
    try
    {
        auto &value = state();
        const std::lock_guard<std::mutex> lock(value.mutex);
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }
        // 先打开新文件，再替换旧文件；打开失败时不会丢掉原本可用的输出目标。
        std::ofstream nextFile(path, std::ios::app);
        if (!nextFile)
        {
            return false;
        }
        value.file = std::move(nextFile);
        return true;
    }
    catch (...)
    {
        // 目录只读或路径非法不应阻止引擎启动，由调用者决定是否提示用户。
        return false;
    }
}

void Log::closeFile()
{
    auto &value = state();
    const std::lock_guard<std::mutex> lock(value.mutex);
    value.file.close();
}

void Log::write(Level level, std::string_view message, const char *file, int line) noexcept
{
    try
    {
        auto &value = state();
        const std::lock_guard<std::mutex> lock(value.mutex);
        if (!accepts(value, level))
        {
            return;
        }

        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
#ifdef _WIN32
        localtime_s(&localTime, &time);
#else
        localtime_r(&time, &localTime);
#endif
        std::ostringstream output;
        output << '[' << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << "] ["
               << levelName(level) << "] [" << fileName(file) << ':' << line << "] "
               << message << '\n';
        const std::string record = output.str();

        if (value.file.is_open())
        {
            value.file << record;
            value.file.flush();
        }
        if (value.consoleEnabled)
        {
            // 统一走stderr，避免Info与Error分属两个流而出现顺序混乱。
            std::cerr << record;
            std::cerr.flush();
        }
    }
    catch (...)
    {
        // 日志是诊断工具，不能替代或覆盖调用者正在处理的异常。
    }
}
