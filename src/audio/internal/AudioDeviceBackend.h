#pragma once

#include "audio/AudioTypes.h"

#include "audio/internal/MiniaudioInclude.h"

#include <array>
#include <memory>

// AudioDeviceBackend封装miniaudio设备、引擎和总线节点。
// 它不保存Voice或Clip，因此可以独立替换后端，而不影响资源和播放状态管理。
class AudioDeviceBackend final
{
public:
    explicit AudioDeviceBackend(const AudioConfig &config);
    ~AudioDeviceBackend();

    AudioDeviceBackend(const AudioDeviceBackend &) = delete;
    AudioDeviceBackend &operator=(const AudioDeviceBackend &) = delete;

    bool isAvailable() const noexcept { return available_; }
    bool usingNullBackend() const noexcept { return nullBackend_; }

    ma_engine *engine() noexcept { return engine_.get(); }
    ma_sound_group *groupFor(AudioBus bus) const noexcept;

    // Mixer状态保存在CPU侧；后端只接收最终生效音量和Listener快照。
    void setBusVolume(AudioBus bus, float volume) noexcept;
    void setListener(const AudioListener &listener) noexcept;

private:
    void initialize(const AudioConfig &config);
    void handleFailure(const AudioConfig &config, const char *operation, ma_result result);
    void uninitialize() noexcept;

    bool available_ = false;
    bool nullBackend_ = false;
    std::unique_ptr<ma_engine> engine_;
    std::array<std::unique_ptr<ma_sound_group>, 4> groups_{};
};
