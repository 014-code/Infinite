#pragma once

#include "audio/AudioTypes.h"

#include <filesystem>
#include <string>

// 音频内部校验和路径规范化工具。
// 这些规则集中在一个模块中，避免AudioSystem、Mixer和资源缓存各自维护不同边界。
namespace AudioValidation
{
    constexpr std::size_t kBusCount = 5;
    constexpr std::size_t kGroupCount = 4;

    std::size_t busIndex(AudioBus bus) noexcept;
    std::size_t groupIndex(AudioBus bus);
    void validateBus(AudioBus bus);
    void validateVolume(float volume, const char *name);
    void validatePitch(float pitch);
    void validateListener(const AudioListener &listener);
    AudioConfig checkedConfig(const AudioConfig &config);

    std::filesystem::path normalizedAudioPath(const std::filesystem::path &path,
        std::size_t maxFileBytes);
    std::string clipKey(const std::filesystem::path &path, AudioLoadMode mode);
}
