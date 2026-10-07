#include "MusicPlayer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

void MusicPlayer::play(std::shared_ptr<const AudioClip> clip, AudioPlayOptions options,
    float fadeSeconds)
{
    if (!clip)
    {
        throw std::invalid_argument("MusicPlayer requires a valid clip");
    }
    if (!std::isfinite(fadeSeconds) || fadeSeconds < 0.0f)
    {
        throw std::invalid_argument("Music fade duration must be finite and non-negative");
    }
    options.bus = AudioBus::Music;
    options.looping = true;

    clearFinishedVoices();
    if (fadingVoice_ != kInvalidAudioVoice)
    {
        audio_->stop(fadingVoice_);
        fadingVoice_ = kInvalidAudioVoice;
    }

    const AudioVoiceId nextVoice = audio_->play(clip, options);
    if (nextVoice == kInvalidAudioVoice)
    {
        return;
    }

    if (currentVoice_ == kInvalidAudioVoice || fadeSeconds == 0.0f)
    {
        if (currentVoice_ != kInvalidAudioVoice)
        {
            audio_->stop(currentVoice_);
        }
        currentVoice_ = nextVoice;
        currentClip_ = std::move(clip);
        currentTargetVolume_ = options.volume;
        audio_->setVoiceVolume(currentVoice_, currentTargetVolume_);
        resetFade();
        return;
    }

    fadingVoice_ = currentVoice_;
    fadingStartVolume_ = currentTargetVolume_;
    currentVoice_ = nextVoice;
    currentClip_ = std::move(clip);
    currentTargetVolume_ = options.volume;
    fadeElapsed_ = 0.0f;
    fadeDuration_ = fadeSeconds;
    audio_->setVoiceVolume(currentVoice_, 0.0f);
}

void MusicPlayer::stop(float fadeSeconds)
{
    if (!std::isfinite(fadeSeconds) || fadeSeconds < 0.0f)
    {
        throw std::invalid_argument("Music fade duration must be finite and non-negative");
    }
    clearFinishedVoices();
    if (fadingVoice_ != kInvalidAudioVoice)
    {
        audio_->stop(fadingVoice_);
        fadingVoice_ = kInvalidAudioVoice;
    }
    if (currentVoice_ == kInvalidAudioVoice)
    {
        currentClip_.reset();
        return;
    }
    if (fadeSeconds == 0.0f)
    {
        audio_->stop(currentVoice_);
        currentVoice_ = kInvalidAudioVoice;
        currentClip_.reset();
        resetFade();
        return;
    }

    fadingVoice_ = currentVoice_;
    fadingStartVolume_ = currentTargetVolume_;
    currentVoice_ = kInvalidAudioVoice;
    currentClip_.reset();
    fadeElapsed_ = 0.0f;
    fadeDuration_ = fadeSeconds;
}

void MusicPlayer::update(float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
    {
        throw std::invalid_argument("Music delta time must be finite and non-negative");
    }
    clearFinishedVoices();
    if (fadeDuration_ <= 0.0f || fadingVoice_ == kInvalidAudioVoice)
    {
        return;
    }

    fadeElapsed_ = std::min(fadeDuration_, fadeElapsed_ + deltaTime);
    const float progress = fadeDuration_ <= 0.0f ? 1.0f : fadeElapsed_ / fadeDuration_;
    if (currentVoice_ != kInvalidAudioVoice)
    {
        audio_->setVoiceVolume(currentVoice_, currentTargetVolume_ * progress);
    }
    audio_->setVoiceVolume(fadingVoice_, fadingStartVolume_ * (1.0f - progress));
    if (progress >= 1.0f)
    {
        audio_->stop(fadingVoice_);
        fadingVoice_ = kInvalidAudioVoice;
        resetFade();
    }
}

bool MusicPlayer::isPlaying() const noexcept
{
    return currentVoice_ != kInvalidAudioVoice && audio_->isPlaying(currentVoice_);
}

void MusicPlayer::clearFinishedVoices() noexcept
{
    if (currentVoice_ != kInvalidAudioVoice && !audio_->isPlaying(currentVoice_))
    {
        currentVoice_ = kInvalidAudioVoice;
        currentClip_.reset();
    }
    if (fadingVoice_ != kInvalidAudioVoice && !audio_->isPlaying(fadingVoice_))
    {
        fadingVoice_ = kInvalidAudioVoice;
    }
}

void MusicPlayer::resetFade() noexcept
{
    fadeElapsed_ = 0.0f;
    fadeDuration_ = 0.0f;
}
