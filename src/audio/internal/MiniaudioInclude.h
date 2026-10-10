#pragma once

// MSYS2通常把头文件安装在miniaudio/miniaudio.h，部分Linux发行版则直接安装
// 为miniaudio.h。统一通过这个内部适配头引入，避免音频实现文件散落平台判断。
#if __has_include(<miniaudio/miniaudio.h>)
#include <miniaudio/miniaudio.h>
#elif __has_include(<miniaudio.h>)
#include <miniaudio.h>
#else
#error "miniaudio headers were not found"
#endif
