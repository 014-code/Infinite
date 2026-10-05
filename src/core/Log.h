#pragma once

#include <filesystem>
#include <string_view>

// 日志只负责记录信息，不决定程序是否退出；无法继续的业务错误仍由异常传递。
namespace Log
{
    // 从低到高排列。比如最低级别为Warning时，只输出Warning和Error。
    enum class Level
    {
        Debug,
        Info,
        Warning,
        Error,
        Off
    };

    // 默认输出Info及以上级别，控制台默认开启，不需要先初始化日志系统。
    void setLevel(Level level);
    bool isEnabled(Level level);
    void setConsoleEnabled(bool enabled);

    // 追加写入文件，不覆盖历史日志；缺少的父目录会自动创建。
    // 打开失败返回false并保留原文件，调用者可以继续使用控制台。
    bool setFile(const std::filesystem::path &path) noexcept;
    void closeFile();

    // 每条日志写完立即刷新，便于程序异常退出后检查最后的记录。
    // 内部串行化输出；输出失败不会抛出异常，避免在处理原错误时再次中断程序。
    void write(Level level, std::string_view message, const char *file, int line) noexcept;
}

// 宏在调用处展开，因此记录的是使用者的文件和行号，而不是Log.cpp的位置。
// do/while让宏像一条普通语句；先过滤再计算message，避免无用的字符串拼接。
#define INFINITE_LOG(level, message) \
    do \
    { \
        if (::Log::isEnabled(level)) \
        { \
            ::Log::write(level, (message), __FILE__, __LINE__); \
        } \
    } while (false)

#define LOG_DEBUG(message) INFINITE_LOG(::Log::Level::Debug, message)
#define LOG_INFO(message) INFINITE_LOG(::Log::Level::Info, message)
#define LOG_WARN(message) INFINITE_LOG(::Log::Level::Warning, message)
#define LOG_ERROR(message) INFINITE_LOG(::Log::Level::Error, message)
