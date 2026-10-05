#pragma once

#include <filesystem>

// 获取可执行文件旁的资源目录，不改变进程当前工作目录。
// Windows使用系统API查询exe位置；其他平台使用传入的启动路径。
std::filesystem::path executableDirectory(const char *executablePath);
