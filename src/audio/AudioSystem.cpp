#include "AudioSystem.h"

#include "audio/internal/AudioClipCache.h"
#include "audio/internal/AudioDeviceBackend.h"
#include "audio/internal/AudioMixerState.h"
#include "audio/internal/AudioValidation.h"
#include "audio/internal/AudioVoicePool.h"

#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace
{
    void validatePlayOptions(const AudioPlayOptions &options)
    {
        AudioValidation::validateBus(options.bus);
        AudioValidation::validateVolume(options.volume, "Audio volume");
        AudioValidation::validatePitch(options.pitch);
        const std::array<float, 3> position{options.x, options.y, options.z};
        for (const float value : position)
        {
            if (!std::isfinite(value))
            {
                throw std::invalid_argument("Audio position values must be finite");
            }
        }
    }
}

struct AudioSystem::Impl
{
    explicit Impl(const AudioConfig &requestedConfig)
        : config(AudioValidation::checkedConfig(requestedConfig)),
          backend(config),
          voices(config.maxVoices)
    {
        // 总线、Listener和暂停状态是纯CPU数据，设备初始化失败时也必须可查询和修改。
        for (std::size_t index = 0; index < AudioValidation::kBusCount; ++index)
        {
            mixer.setBusVolume(static_cast<AudioBus>(index), 1.0f);
        }
    }

    AudioConfig config;
    // 声明顺序很重要：Impl销毁时VoicePool先于Backend，保证ma_sound释放时engine仍有效。
    AudioDeviceBackend backend;
    AudioClipCache clips;
    AudioVoicePool voices;
    AudioMixerState mixer;
};

AudioSystem::AudioSystem(const AudioConfig &config) : impl_(std::make_unique<Impl>(config))
{
}

AudioSystem::~AudioSystem() = default;

AudioSystem::AudioSystem(AudioSystem &&other) noexcept = default;

AudioSystem &AudioSystem::operator=(AudioSystem &&other) noexcept = default;

bool AudioSystem::isAvailable() const noexcept
{
    return impl_->backend.isAvailable();
}

bool AudioSystem::usingNullBackend() const noexcept
{
    return impl_->backend.usingNullBackend();
}

std::shared_ptr<AudioClip> AudioSystem::loadClip(const std::filesystem::path &path,
    const AudioClipOptions &options)
{
    return impl_->clips.load(path, options);
}

AudioVoiceId AudioSystem::play(const std::shared_ptr<const AudioClip> &clip,
    const AudioPlayOptions &options)
{
    if (!clip)
    {
        throw std::invalid_argument("Audio play requires a valid clip");
    }
    validatePlayOptions(options);
    return impl_->voices.play(impl_->backend, clip, options, impl_->mixer.isPaused());
}

void AudioSystem::stop(AudioVoiceId voice) noexcept
{
    impl_->voices.stop(voice);
}

void AudioSystem::pause(AudioVoiceId voice) noexcept
{
    impl_->voices.pause(voice);
}

void AudioSystem::resume(AudioVoiceId voice) noexcept
{
    impl_->voices.resume(voice, impl_->mixer.isPaused());
}

void AudioSystem::setVoiceVolume(AudioVoiceId voice, float volume)
{
    impl_->voices.setVolume(voice, volume);
}

void AudioSystem::setVoicePitch(AudioVoiceId voice, float pitch)
{
    impl_->voices.setPitch(voice, pitch);
}

void AudioSystem::setVoiceLooping(AudioVoiceId voice, bool looping) noexcept
{
    impl_->voices.setLooping(voice, looping);
}

void AudioSystem::setVoiceSpatial(AudioVoiceId voice, bool spatial) noexcept
{
    impl_->voices.setSpatial(voice, spatial);
}

void AudioSystem::setVoicePosition(AudioVoiceId voice, float x, float y, float z) noexcept
{
    impl_->voices.setPosition(voice, x, y, z);
}

bool AudioSystem::isPlaying(AudioVoiceId voice) const noexcept
{
    return impl_->voices.isPlaying(voice);
}

AudioVoiceState AudioSystem::state(AudioVoiceId voice) const noexcept
{
    return impl_->voices.state(voice);
}

void AudioSystem::stopAll() noexcept
{
    impl_->voices.stopAll();
}

void AudioSystem::setBusVolume(AudioBus bus, float volume)
{
    impl_->mixer.setBusVolume(bus, volume);
    impl_->backend.setBusVolume(bus, impl_->mixer.effectiveBusVolume(bus));
}

float AudioSystem::busVolume(AudioBus bus) const noexcept
{
    return impl_->mixer.busVolume(bus);
}

void AudioSystem::setBusMuted(AudioBus bus, bool muted)
{
    impl_->mixer.setBusMuted(bus, muted);
    impl_->backend.setBusVolume(bus, impl_->mixer.effectiveBusVolume(bus));
}

bool AudioSystem::isBusMuted(AudioBus bus) const noexcept
{
    return impl_->mixer.isBusMuted(bus);
}

void AudioSystem::update(float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
    {
        throw std::invalid_argument("Audio delta time must be finite and non-negative");
    }
    impl_->voices.update(deltaTime);
}

void AudioSystem::setListener(const AudioListener &listener)
{
    impl_->mixer.setListener(listener);
    impl_->backend.setListener(impl_->mixer.listener());
}

const AudioListener &AudioSystem::listener() const noexcept
{
    return impl_->mixer.listener();
}

void AudioSystem::setPaused(bool paused) noexcept
{
    impl_->mixer.setPaused(paused);
    impl_->voices.setPaused(paused);
}

bool AudioSystem::isPaused() const noexcept
{
    return impl_->mixer.isPaused();
}

std::size_t AudioSystem::activeVoiceCount() const noexcept
{
    return impl_->voices.activeCount();
}

std::size_t AudioSystem::clipCount() const noexcept
{
    return impl_->clips.size();
}
