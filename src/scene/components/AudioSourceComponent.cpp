#include "AudioSourceComponent.h"

#include "math/Transform.h"

#include <cmath>
#include <stdexcept>

AudioSourceComponent::~AudioSourceComponent()
{
    detach();
}

void AudioSourceComponent::attach(AudioSystem &audio, std::shared_ptr<const AudioClip> clip)
{
    if (!clip)
    {
        throw std::invalid_argument("Audio source requires a valid clip");
    }
    detach();
    audio_ = &audio;
    clip_ = std::move(clip);
}

void AudioSourceComponent::detach() noexcept
{
    stop();
    audio_ = nullptr;
    clip_.reset();
}

void AudioSourceComponent::setLooping(bool looping) noexcept
{
    looping_ = looping;
    if (audio_ && voice_ != kInvalidAudioVoice)
    {
        audio_->setVoiceLooping(voice_, looping_);
    }
}

void AudioSourceComponent::setVolume(float volume)
{
    // 即使当前没有播放Voice，也要尽早拒绝非法配置，避免play时才发现问题。
    if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f)
    {
        throw std::invalid_argument("Audio source volume must be finite and in [0,1]");
    }
    volume_ = volume;
    if (audio_ && voice_ != kInvalidAudioVoice)
    {
        audio_->setVoiceVolume(voice_, volume_);
    }
}

void AudioSourceComponent::setPitch(float pitch)
{
    if (!std::isfinite(pitch) || pitch <= 0.0f)
    {
        throw std::invalid_argument("Audio source pitch must be positive");
    }
    pitch_ = pitch;
    if (audio_ && voice_ != kInvalidAudioVoice)
    {
        audio_->setVoicePitch(voice_, pitch_);
    }
}

void AudioSourceComponent::setSpatial(bool spatial) noexcept
{
    spatial_ = spatial;
    if (audio_ && voice_ != kInvalidAudioVoice)
    {
        audio_->setVoiceSpatial(voice_, spatial_);
    }
}

void AudioSourceComponent::play()
{
    if (!audio_ || !clip_)
    {
        throw std::logic_error("Audio source must be attached before play");
    }
    if (voice_ != kInvalidAudioVoice)
    {
        stop();
    }
    AudioPlayOptions options;
    options.bus = bus_;
    options.volume = volume_;
    options.pitch = pitch_;
    options.looping = looping_;
    options.spatial = spatial_;
    voice_ = audio_->play(clip_, options);
}

void AudioSourceComponent::stop() noexcept
{
    if (audio_ && voice_ != kInvalidAudioVoice)
    {
        audio_->stop(voice_);
    }
    voice_ = kInvalidAudioVoice;
}

void AudioSourceComponent::pause() noexcept
{
    if (audio_ && voice_ != kInvalidAudioVoice)
    {
        audio_->pause(voice_);
    }
}

void AudioSourceComponent::resume() noexcept
{
    if (audio_ && voice_ != kInvalidAudioVoice)
    {
        audio_->resume(voice_);
    }
}

bool AudioSourceComponent::isPlaying() const noexcept
{
    return audio_ && voice_ != kInvalidAudioVoice && audio_->isPlaying(voice_);
}

void AudioSourceComponent::syncTransform(const Transform &transform) noexcept
{
    if (!spatial_ || !audio_ || voice_ == kInvalidAudioVoice)
    {
        return;
    }
    const auto &position = transform.position;
    audio_->setVoicePosition(voice_, position.x, position.y, position.z);
}
