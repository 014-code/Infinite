#pragma once

#include "AudioClip.h"
#include "AudioTypes.h"
#include "AudioVoice.h"

#include <filesystem>
#include <memory>

// AudioSystem管理设备、音频资源缓存、播放实例和混音总线。
//
// 约束：
// 1. 所有公开方法在主线程调用；底层音频设备线程由后端自己管理。
// 2. AudioClip由shared_ptr共享；AudioSystem不会因为清理缓存而使外部Clip失效。
// 3. AudioVoiceId只是句柄，stop后句柄立即失效，不能继续使用。
class AudioSystem final
{
public:
    explicit AudioSystem(const AudioConfig &config = {});
    ~AudioSystem();

    AudioSystem(const AudioSystem &) = delete;
    AudioSystem &operator=(const AudioSystem &) = delete;
    AudioSystem(AudioSystem &&other) noexcept;
    AudioSystem &operator=(AudioSystem &&other) noexcept;

    // 音频设备是否真正可用。使用Null Backend时返回false，但系统仍可安全调用其他接口。
    bool isAvailable() const noexcept;
    bool usingNullBackend() const noexcept;

    // 读取并缓存文件描述。第一次调用会校验路径和文件大小，后续相同路径/模式复用Clip。
    std::shared_ptr<AudioClip> loadClip(const std::filesystem::path &path,
        const AudioClipOptions &options = {});

    // 创建一次播放实例。失败时返回kInvalidAudioVoice；无声后端仍返回可测试的逻辑句柄。
    AudioVoiceId play(const std::shared_ptr<const AudioClip> &clip,
        const AudioPlayOptions &options = {});
    void stop(AudioVoiceId voice) noexcept;
    void pause(AudioVoiceId voice) noexcept;
    void resume(AudioVoiceId voice) noexcept;
    void setVoiceVolume(AudioVoiceId voice, float volume);
    void setVoicePitch(AudioVoiceId voice, float pitch);
    void setVoiceLooping(AudioVoiceId voice, bool looping) noexcept;
    void setVoiceSpatial(AudioVoiceId voice, bool spatial) noexcept;
    void setVoicePosition(AudioVoiceId voice, float x, float y, float z) noexcept;
    bool isPlaying(AudioVoiceId voice) const noexcept;
    AudioVoiceState state(AudioVoiceId voice) const noexcept;

    // 停止全部播放实例；状态切换和应用退出时使用。
    void stopAll() noexcept;

    void setBusVolume(AudioBus bus, float volume);
    float busVolume(AudioBus bus) const noexcept;
    void setBusMuted(AudioBus bus, bool muted);
    bool isBusMuted(AudioBus bus) const noexcept;

    // 更新音频播放实例状态。应每帧调用一次，不要放进固定物理步，避免一帧多个子步重复更新。
    void update(float deltaTime);

    // 设置唯一Listener；空间音频关闭时仍可以提前更新，后续开启空间化即可使用。
    void setListener(const AudioListener &listener);
    const AudioListener &listener() const noexcept;

    // 暂停/恢复所有当前Voice，不改变Clip缓存和总线音量。
    void setPaused(bool paused) noexcept;
    bool isPaused() const noexcept;

    std::size_t activeVoiceCount() const noexcept;
    std::size_t clipCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
