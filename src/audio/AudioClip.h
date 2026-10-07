#pragma once

#include "AudioTypes.h"

#include <filesystem>
#include <utility>

// AudioClip只描述一份可共享的音频文件资源，不代表某一次播放。
// 它不暴露miniaudio等第三方类型，后端替换不会影响应用层代码。
class AudioClip final
{
public:
    const std::filesystem::path &path() const noexcept { return path_; }
    AudioLoadMode loadMode() const noexcept { return loadMode_; }

private:
    friend class AudioSystem;

    AudioClip(std::filesystem::path path, AudioLoadMode loadMode)
        : path_(std::move(path)), loadMode_(loadMode)
    {
    }

    std::filesystem::path path_;
    AudioLoadMode loadMode_;
};
