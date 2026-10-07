#pragma once

#include "Component.h"
#include "audio/AudioSystem.h"

#include <memory>

class Transform;

// AudioSourceComponent表示一个场景物体上的持续声音来源。
//
// 它只保存Clip、播放参数和当前Voice句柄；设备、混音总线和实际播放由AudioSystem负责。
// 一次性跳跃/点击音效不需要创建这个组件，直接调用AudioSystem::play即可。
class AudioSourceComponent final : public Component
{
public:
    AudioSourceComponent() = default;
    explicit AudioSourceComponent(GameObject &owner) noexcept : Component(owner) {}
    ~AudioSourceComponent() override;

    AudioSourceComponent(const AudioSourceComponent &) = delete;
    AudioSourceComponent &operator=(const AudioSourceComponent &) = delete;
    AudioSourceComponent(AudioSourceComponent &&) = delete;
    AudioSourceComponent &operator=(AudioSourceComponent &&) = delete;

    // 绑定音频系统和Clip。重复绑定会先停止当前Voice，再替换配置。
    void attach(AudioSystem &audio, std::shared_ptr<const AudioClip> clip);
    void detach() noexcept;

    void setLooping(bool looping) noexcept;
    bool isLooping() const noexcept { return looping_; }
    void setVolume(float volume);
    float volume() const noexcept { return volume_; }
    void setPitch(float pitch);
    float pitch() const noexcept { return pitch_; }
    void setSpatial(bool spatial) noexcept;
    bool isSpatial() const noexcept { return spatial_; }
    // 总线配置在下一次play时生效；不强行中断当前Voice，避免修改配置意外重启声音。
    void setBus(AudioBus bus) noexcept { bus_ = bus; }
    AudioBus bus() const noexcept { return bus_; }

    void play();
    void stop() noexcept;
    void pause() noexcept;
    void resume() noexcept;
    bool isPlaying() const noexcept;

    // Scene在每帧Transform更新后调用；非空间音源不需要同步位置。
    void syncTransform(const Transform &transform) noexcept;

    const std::shared_ptr<const AudioClip> &clip() const noexcept { return clip_; }
    AudioVoiceId voice() const noexcept { return voice_; }

private:
    AudioSystem *audio_ = nullptr;
    std::shared_ptr<const AudioClip> clip_;
    AudioVoiceId voice_ = kInvalidAudioVoice;
    AudioBus bus_ = AudioBus::Sfx;
    float volume_ = 1.0f;
    float pitch_ = 1.0f;
    bool looping_ = false;
    bool spatial_ = false;
};
