#pragma once

#include "audio/AudioClip.h"
#include "audio/AudioVoice.h"
#include "audio/AudioTypes.h"

#include <miniaudio/miniaudio.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class AudioDeviceBackend;

// AudioVoicePool负责播放实例的句柄、状态机和miniaudio sound对象。
// Clip缓存、总线状态和设备生命周期不在这里管理，便于以后替换Voice分配策略。
class AudioVoicePool final
{
public:
    explicit AudioVoicePool(std::size_t maximumVoices);
    ~AudioVoicePool();

    AudioVoicePool(const AudioVoicePool &) = delete;
    AudioVoicePool &operator=(const AudioVoicePool &) = delete;

    AudioVoiceId play(AudioDeviceBackend &backend, const std::shared_ptr<const AudioClip> &clip,
        const AudioPlayOptions &options, bool globallyPaused);
    void stop(AudioVoiceId voice) noexcept;
    void pause(AudioVoiceId voice) noexcept;
    void resume(AudioVoiceId voice, bool globallyPaused) noexcept;
    void setVolume(AudioVoiceId voice, float volume);
    void setPitch(AudioVoiceId voice, float pitch);
    void setLooping(AudioVoiceId voice, bool looping) noexcept;
    void setSpatial(AudioVoiceId voice, bool spatial) noexcept;
    void setPosition(AudioVoiceId voice, float x, float y, float z) noexcept;
    bool isPlaying(AudioVoiceId voice) const noexcept;
    AudioVoiceState state(AudioVoiceId voice) const noexcept;
    void stopAll() noexcept;
    void update(float deltaTime);
    void setPaused(bool paused) noexcept;

    std::size_t activeCount() const noexcept { return activeVoices_; }

private:
    struct VoiceSlot
    {
        std::uint32_t generation = 0;
        bool used = false;
        bool manuallyPaused = false;
        bool pausedBySystem = false;
        bool looping = false;
        AudioBus bus = AudioBus::Sfx;
        AudioVoiceState state = AudioVoiceState::Invalid;
        float volume = 1.0f;
        float pitch = 1.0f;
        bool spatial = false;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        std::shared_ptr<const AudioClip> clip;
        std::unique_ptr<ma_sound> sound;
    };

    std::size_t allocateVoice();
    AudioVoiceId idFor(std::size_t index) const noexcept;
    VoiceSlot *find(AudioVoiceId voice) noexcept;
    const VoiceSlot *find(AudioVoiceId voice) const noexcept;
    void release(VoiceSlot &slot) noexcept;

    std::size_t maximumVoices_;
    std::vector<VoiceSlot> voices_;
    std::size_t activeVoices_ = 0;
    bool globallyPaused_ = false;
};
