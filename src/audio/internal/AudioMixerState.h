#pragma once

#include "audio/AudioTypes.h"

#include <array>

// AudioMixerState只保存音频系统的CPU侧混音状态，不直接调用miniaudio。
// AudioSystem在状态改变后把结果提交给AudioDeviceBackend。
class AudioMixerState final
{
public:
    AudioMixerState();

    void setBusVolume(AudioBus bus, float volume);
    float busVolume(AudioBus bus) const noexcept;
    void setBusMuted(AudioBus bus, bool muted);
    bool isBusMuted(AudioBus bus) const noexcept;

    void setListener(const AudioListener &listener);
    const AudioListener &listener() const noexcept { return listener_; }

    void setPaused(bool paused) noexcept { paused_ = paused; }
    bool isPaused() const noexcept { return paused_; }

    bool busMuted(AudioBus bus) const noexcept;
    float effectiveBusVolume(AudioBus bus) const noexcept;

private:
    struct BusState
    {
        float volume = 1.0f;
        bool muted = false;
    };

    std::array<BusState, 5> buses_{};
    AudioListener listener_{};
    bool paused_ = false;
};
