#include "AudioMixerState.h"

#include "audio/internal/AudioValidation.h"

AudioMixerState::AudioMixerState() = default;

void AudioMixerState::setBusVolume(AudioBus bus, float volume)
{
    AudioValidation::validateBus(bus);
    AudioValidation::validateVolume(volume, "Audio bus volume");
    buses_[AudioValidation::busIndex(bus)].volume = volume;
}

float AudioMixerState::busVolume(AudioBus bus) const noexcept
{
    const auto index = AudioValidation::busIndex(bus);
    return index < AudioValidation::kBusCount ? buses_[index].volume : 0.0f;
}

void AudioMixerState::setBusMuted(AudioBus bus, bool muted)
{
    AudioValidation::validateBus(bus);
    buses_[AudioValidation::busIndex(bus)].muted = muted;
}

bool AudioMixerState::isBusMuted(AudioBus bus) const noexcept
{
    const auto index = AudioValidation::busIndex(bus);
    return index < AudioValidation::kBusCount && buses_[index].muted;
}

void AudioMixerState::setListener(const AudioListener &listener)
{
    AudioValidation::validateListener(listener);
    listener_ = listener;
}

bool AudioMixerState::busMuted(AudioBus bus) const noexcept
{
    const auto index = AudioValidation::busIndex(bus);
    return index < AudioValidation::kBusCount && buses_[index].muted;
}

float AudioMixerState::effectiveBusVolume(AudioBus bus) const noexcept
{
    return busMuted(bus) ? 0.0f : busVolume(bus);
}
