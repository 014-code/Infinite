#pragma once

#include "AudioSystem.h"

#include <memory>

// MusicPlayer管理“当前音乐”和交叉淡入淡出，不承担音频资源查找。
// AudioClip仍由AudioSystem缓存，播放器只持有共享引用和两个Voice句柄。
class MusicPlayer final
{
public:
    explicit MusicPlayer(AudioSystem &audio) noexcept : audio_(&audio) {}

    MusicPlayer(const MusicPlayer &) = delete;
    MusicPlayer &operator=(const MusicPlayer &) = delete;

    // fadeSeconds为0时立即切换；大于0时旧音乐淡出、新音乐淡入。
    void play(std::shared_ptr<const AudioClip> clip, AudioPlayOptions options = {},
        float fadeSeconds = 0.0f);
    void stop(float fadeSeconds = 0.0f);
    void update(float deltaTime);

    bool isPlaying() const noexcept;
    AudioVoiceId voice() const noexcept { return currentVoice_; }
    const std::shared_ptr<const AudioClip> &clip() const noexcept { return currentClip_; }

private:
    void clearFinishedVoices() noexcept;
    void resetFade() noexcept;

    AudioSystem *audio_ = nullptr;
    std::shared_ptr<const AudioClip> currentClip_;
    AudioVoiceId currentVoice_ = kInvalidAudioVoice;
    AudioVoiceId fadingVoice_ = kInvalidAudioVoice;
    float currentTargetVolume_ = 1.0f;
    float fadingStartVolume_ = 1.0f;
    float fadeElapsed_ = 0.0f;
    float fadeDuration_ = 0.0f;
};
