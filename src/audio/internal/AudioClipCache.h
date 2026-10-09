#pragma once

#include "audio/AudioClip.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

// AudioClipCache负责音频文件路径校验和共享资源缓存。
// 缓存清理不会使调用方仍持有的shared_ptr失效。
class AudioClipCache final
{
public:
    std::shared_ptr<AudioClip> load(const std::filesystem::path &path,
        const AudioClipOptions &options);
    std::size_t size() const noexcept { return clips_.size(); }

private:
    std::unordered_map<std::string, std::shared_ptr<AudioClip>> clips_;
};
