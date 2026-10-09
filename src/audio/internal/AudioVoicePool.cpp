#include "AudioVoicePool.h"

#include "audio/internal/AudioDeviceBackend.h"
#include "audio/internal/AudioValidation.h"
#include "core/Log.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

AudioVoicePool::AudioVoicePool(std::size_t maximumVoices)
    : maximumVoices_(maximumVoices)
{
    if (maximumVoices_ == 0)
    {
        throw std::invalid_argument("Audio maxVoices must be positive");
    }
}

AudioVoicePool::~AudioVoicePool()
{
    stopAll();
}

std::size_t AudioVoicePool::allocateVoice()
{
    for (std::size_t index = 0; index < voices_.size(); ++index)
    {
        if (!voices_[index].used)
        {
            auto &slot = voices_[index];
            slot.generation = slot.generation == std::numeric_limits<std::uint32_t>::max()
                ? 1
                : slot.generation + 1;
            slot.used = true;
            ++activeVoices_;
            return index;
        }
    }

    if (voices_.size() >= maximumVoices_)
    {
        return std::numeric_limits<std::size_t>::max();
    }

    voices_.emplace_back();
    auto &slot = voices_.back();
    slot.generation = 1;
    slot.used = true;
    ++activeVoices_;
    return voices_.size() - 1;
}

AudioVoiceId AudioVoicePool::idFor(std::size_t index) const noexcept
{
    return (static_cast<AudioVoiceId>(voices_[index].generation) << 32) |
        static_cast<AudioVoiceId>(index + 1);
}

AudioVoicePool::VoiceSlot *AudioVoicePool::find(AudioVoiceId voice) noexcept
{
    if (voice == kInvalidAudioVoice)
    {
        return nullptr;
    }
    const auto indexValue = static_cast<std::uint32_t>(voice & 0xFFFFFFFFu);
    const auto generation = static_cast<std::uint32_t>(voice >> 32);
    if (indexValue == 0)
    {
        return nullptr;
    }
    const auto index = static_cast<std::size_t>(indexValue - 1);
    if (index >= voices_.size())
    {
        return nullptr;
    }
    auto &slot = voices_[index];
    if (!slot.used || slot.generation != generation)
    {
        return nullptr;
    }
    return &slot;
}

const AudioVoicePool::VoiceSlot *AudioVoicePool::find(AudioVoiceId voice) const noexcept
{
    return const_cast<AudioVoicePool *>(this)->find(voice);
}

void AudioVoicePool::release(VoiceSlot &slot) noexcept
{
    if (slot.sound)
    {
        ma_sound_uninit(slot.sound.get());
        slot.sound.reset();
    }
    slot.clip.reset();
    slot.used = false;
    slot.manuallyPaused = false;
    slot.pausedBySystem = false;
    slot.looping = false;
    slot.volume = 1.0f;
    slot.pitch = 1.0f;
    slot.spatial = false;
    slot.x = 0.0f;
    slot.y = 0.0f;
    slot.z = 0.0f;
    slot.state = AudioVoiceState::Invalid;
    if (activeVoices_ > 0)
    {
        --activeVoices_;
    }
}

AudioVoiceId AudioVoicePool::play(AudioDeviceBackend &backend,
    const std::shared_ptr<const AudioClip> &clip, const AudioPlayOptions &options,
    bool globallyPaused)
{
    const auto voiceIndex = allocateVoice();
    if (voiceIndex == std::numeric_limits<std::size_t>::max())
    {
        LOG_WARN("Audio voice limit reached; play request was ignored");
        return kInvalidAudioVoice;
    }

    auto &slot = voices_[voiceIndex];
    slot.bus = options.bus;
    slot.looping = options.looping;
    slot.volume = options.volume;
    slot.pitch = options.pitch;
    slot.spatial = options.spatial;
    slot.x = options.x;
    slot.y = options.y;
    slot.z = options.z;
    slot.clip = clip;
    slot.manuallyPaused = false;
    slot.pausedBySystem = globallyPaused;

    if (backend.isAvailable())
    {
        slot.sound = std::make_unique<ma_sound>();
        const ma_uint32 flags = clip->loadMode() == AudioLoadMode::Streaming
            ? MA_SOUND_FLAG_STREAM
            : MA_SOUND_FLAG_DECODE;

        ma_result result = MA_ERROR;
#if defined(_WIN32)
        const auto widePath = clip->path().wstring();
        result = ma_sound_init_from_file_w(backend.engine(), widePath.c_str(), flags,
            backend.groupFor(options.bus), nullptr, slot.sound.get());
#else
        const auto utf8Path = clip->path().u8string();
        result = ma_sound_init_from_file(backend.engine(), utf8Path.c_str(), flags,
            backend.groupFor(options.bus), nullptr, slot.sound.get());
#endif
        if (result != MA_SUCCESS)
        {
            LOG_WARN("Failed to load audio clip for playback: " + clip->path().u8string() +
                ", miniaudio result " + std::to_string(static_cast<int>(result)));
            release(slot);
            return kInvalidAudioVoice;
        }
        ma_sound_set_volume(slot.sound.get(), options.volume);
        ma_sound_set_pitch(slot.sound.get(), options.pitch);
        ma_sound_set_looping(slot.sound.get(), options.looping ? MA_TRUE : MA_FALSE);
        ma_sound_set_spatialization_enabled(slot.sound.get(), options.spatial ? MA_TRUE : MA_FALSE);
        if (options.spatial)
        {
            ma_sound_set_positioning(slot.sound.get(), ma_positioning_absolute);
            ma_sound_set_position(slot.sound.get(), options.x, options.y, options.z);
        }
    }

    if (globallyPaused)
    {
        slot.state = AudioVoiceState::Paused;
        return idFor(voiceIndex);
    }

    if (slot.sound && ma_sound_start(slot.sound.get()) != MA_SUCCESS)
    {
        LOG_WARN("Failed to start audio clip: " + clip->path().u8string());
        release(slot);
        return kInvalidAudioVoice;
    }
    slot.state = AudioVoiceState::Playing;
    return idFor(voiceIndex);
}

void AudioVoicePool::stop(AudioVoiceId voice) noexcept
{
    if (auto *slot = find(voice))
    {
        release(*slot);
    }
}

void AudioVoicePool::pause(AudioVoiceId voice) noexcept
{
    if (auto *slot = find(voice); slot && slot->state == AudioVoiceState::Playing)
    {
        if (slot->sound)
        {
            ma_sound_stop(slot->sound.get());
        }
        slot->manuallyPaused = true;
        slot->state = AudioVoiceState::Paused;
    }
}

void AudioVoicePool::resume(AudioVoiceId voice, bool globallyPaused) noexcept
{
    if (auto *slot = find(voice); slot && slot->state == AudioVoiceState::Paused)
    {
        slot->manuallyPaused = false;
        if (globallyPaused)
        {
            slot->pausedBySystem = true;
            return;
        }
        if (slot->sound && ma_sound_start(slot->sound.get()) != MA_SUCCESS)
        {
            release(*slot);
            return;
        }
        slot->pausedBySystem = false;
        slot->state = AudioVoiceState::Playing;
    }
}

void AudioVoicePool::setVolume(AudioVoiceId voice, float volume)
{
    AudioValidation::validateVolume(volume, "Audio voice volume");
    if (auto *slot = find(voice))
    {
        slot->volume = volume;
        if (slot->sound)
        {
            ma_sound_set_volume(slot->sound.get(), volume);
        }
    }
}

void AudioVoicePool::setPitch(AudioVoiceId voice, float pitch)
{
    AudioValidation::validatePitch(pitch);
    if (auto *slot = find(voice))
    {
        slot->pitch = pitch;
        if (slot->sound)
        {
            ma_sound_set_pitch(slot->sound.get(), pitch);
        }
    }
}

void AudioVoicePool::setLooping(AudioVoiceId voice, bool looping) noexcept
{
    if (auto *slot = find(voice))
    {
        slot->looping = looping;
        if (slot->sound)
        {
            ma_sound_set_looping(slot->sound.get(), looping ? MA_TRUE : MA_FALSE);
        }
    }
}

void AudioVoicePool::setSpatial(AudioVoiceId voice, bool spatial) noexcept
{
    if (auto *slot = find(voice))
    {
        slot->spatial = spatial;
        if (slot->sound)
        {
            ma_sound_set_spatialization_enabled(slot->sound.get(), spatial ? MA_TRUE : MA_FALSE);
            if (spatial)
            {
                ma_sound_set_positioning(slot->sound.get(), ma_positioning_absolute);
                ma_sound_set_position(slot->sound.get(), slot->x, slot->y, slot->z);
            }
        }
    }
}

void AudioVoicePool::setPosition(AudioVoiceId voice, float x, float y, float z) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    {
        return;
    }
    if (auto *slot = find(voice))
    {
        slot->x = x;
        slot->y = y;
        slot->z = z;
        if (slot->sound && slot->spatial)
        {
            ma_sound_set_position(slot->sound.get(), x, y, z);
        }
    }
}

bool AudioVoicePool::isPlaying(AudioVoiceId voice) const noexcept
{
    const auto *slot = find(voice);
    if (!slot || slot->state != AudioVoiceState::Playing)
    {
        return false;
    }
    return !slot->sound || ma_sound_is_playing(slot->sound.get()) == MA_TRUE;
}

AudioVoiceState AudioVoicePool::state(AudioVoiceId voice) const noexcept
{
    const auto *slot = find(voice);
    if (!slot)
    {
        return AudioVoiceState::Invalid;
    }
    if (slot->state == AudioVoiceState::Playing && slot->sound &&
        ma_sound_is_playing(slot->sound.get()) != MA_TRUE)
    {
        return AudioVoiceState::Stopped;
    }
    return slot->state;
}

void AudioVoicePool::stopAll() noexcept
{
    for (auto &slot : voices_)
    {
        if (slot.used)
        {
            release(slot);
        }
    }
}

void AudioVoicePool::update(float deltaTime)
{
    (void)deltaTime;
    for (auto &slot : voices_)
    {
        if (slot.used && slot.sound && !slot.looping && ma_sound_at_end(slot.sound.get()) == MA_TRUE)
        {
            release(slot);
        }
    }
}

void AudioVoicePool::setPaused(bool paused) noexcept
{
    if (globallyPaused_ == paused)
    {
        return;
    }
    globallyPaused_ = paused;
    for (auto &slot : voices_)
    {
        if (!slot.used)
        {
            continue;
        }
        if (paused)
        {
            if (slot.state == AudioVoiceState::Playing)
            {
                if (slot.sound)
                {
                    ma_sound_stop(slot.sound.get());
                }
                slot.pausedBySystem = true;
                slot.state = AudioVoiceState::Paused;
            }
        }
        else if (slot.pausedBySystem && !slot.manuallyPaused)
        {
            if (slot.sound && ma_sound_start(slot.sound.get()) != MA_SUCCESS)
            {
                release(slot);
                continue;
            }
            slot.pausedBySystem = false;
            slot.state = AudioVoiceState::Playing;
        }
    }
}
