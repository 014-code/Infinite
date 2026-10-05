#include "ResourcePath.h"

#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

std::filesystem::path executableDirectory(const char *executablePath)
{
#ifdef _WIN32
    // 使用宽字符路径，避免Windows中文目录在查询时丢失。
    (void)executablePath;
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
    {
        throw std::runtime_error("Failed to locate executable directory");
    }
    path.resize(length);
    return std::filesystem::path(path).parent_path();
#else
    return std::filesystem::absolute(executablePath).parent_path();
#endif
}
