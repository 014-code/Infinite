#include "AudioSystem.h"

#include "core/Log.h"

#include <miniaudio/miniaudio.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    constexpr std::size_t kBusCount = 5;
    constexpr std::size_t kGroupCount = 4;

    std::size_t busIndex(AudioBus bus)
    {
        return static_cast<std::size_t>(bus);
    }

    void validateBus(AudioBus bus)
    {
        if (busIndex(bus) >= kBusCount)
        {
            throw std::invalid_argument("Invalid audio bus");
        }
    }

    std::size_t groupIndex(AudioBus bus)
    {
        if (bus == AudioBus::Master)
        {
            throw std::invalid_argument("Master bus does not have a sound group");
        }
        return busIndex(bus) - 1;
    }

    void validateVolume(float volume, const char *name)
    {
        if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f)
        {
            throw std::invalid_argument(std::string(name) + " must be finite and in [0,1]");
        }
    }

    void validatePitch(float pitch)
    {
        if (!std::isfinite(pitch) || pitch <= 0.0f)
        {
            throw std::invalid_argument("Audio pitch must be finite and positive");
        }
    }

    void validateListener(const AudioListener &listener)
    {
        const std::array<float, 9> values{
            listener.positionX, listener.positionY, listener.positionZ,
            listener.forwardX, listener.forwardY, listener.forwardZ,
            listener.upX, listener.upY, listener.upZ};
        for (const float value : values)
        {
            if (!std::isfinite(value))
            {
                throw std::invalid_argument("Audio listener values must be finite");
            }
        }
    }

    std::filesystem::path normalizedAudioPath(const std::filesystem::path &path,
        std::size_t maxFileBytes)
    {
        if (path.empty())
        {
            throw std::invalid_argument("Audio path must not be empty");
        }
        if (maxFileBytes == 0)
        {
            throw std::invalid_argument("Audio file size limit must be positive");
        }

        std::error_code error;
        const auto absolute = std::filesystem::absolute(path, error);
        if (error)
        {
            throw std::runtime_error("Failed to normalize audio path: " + error.message());
        }
        const auto normalized = absolute.lexically_normal();
        if (!std::filesystem::is_regular_file(normalized, error) || error)
        {
            throw std::runtime_error("Audio file does not exist: " + normalized.u8string());
        }

        const auto fileSize = std::filesystem::file_size(normalized, error);
        if (error)
        {
            throw std::runtime_error("Failed to inspect audio file: " + error.message());
        }
        if (fileSize > maxFileBytes)
        {
            throw std::runtime_error("Audio file exceeds the configured size limit: " +
                normalized.u8string());
        }
        return normalized;
    }

    std::string clipKey(const std::filesystem::path &path, AudioLoadMode mode)
    {
        return path.u8string() + "|" + std::to_string(static_cast<int>(mode));
    }
}

struct AudioSystem::Impl
{
    struct BusState
    {
        float volume = 1.0f;
        bool muted = false;
    };

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

    explicit Impl(const AudioConfig &requestedConfig) : config(requestedConfig)
    {
        if (config.maxVoices == 0)
        {
            throw std::invalid_argument("Audio maxVoices must be positive");
        }

        buses[busIndex(AudioBus::Master)].volume = 1.0f;
        buses[busIndex(AudioBus::Music)].volume = 1.0f;
        buses[busIndex(AudioBus::Sfx)].volume = 1.0f;
        buses[busIndex(AudioBus::Ui)].volume = 1.0f;
        buses[busIndex(AudioBus::Ambient)].volume = 1.0f;

        if (!config.enabled)
        {
            nullBackend = true;
            return;
        }

        initializeBackend();
    }

    ~Impl()
    {
        stopAll();
        uninitializeBackend();
    }

    void initializeBackend()
    {
        engine = std::make_unique<ma_engine>();
        ma_engine_config engineConfig = ma_engine_config_init();
        const ma_result engineResult = ma_engine_init(&engineConfig, engine.get());
        if (engineResult != MA_SUCCESS)
        {
            engine.reset();
            handleBackendFailure("audio device initialization", engineResult);
            return;
        }

        for (std::size_t index = 0; index < kGroupCount; ++index)
        {
            auto group = std::make_unique<ma_sound_group>();
            const ma_result groupResult = ma_sound_group_init(
                engine.get(), 0, nullptr, group.get());
            if (groupResult != MA_SUCCESS)
            {
                uninitializeBackend();
                handleBackendFailure("audio bus initialization", groupResult);
                return;
            }
            groups[index] = std::move(group);
        }

        available = true;
        nullBackend = false;
        applyAllBusVolumes();
        applyListener();
    }

    void handleBackendFailure(const char *operation, ma_result result)
    {
        const std::string message = std::string(operation) + " failed with miniaudio result " +
            std::to_string(static_cast<int>(result));
        if (!config.allowNullBackend)
        {
            throw std::runtime_error(message);
        }
        LOG_WARN(message + "; using the null audio backend");
        available = false;
        nullBackend = true;
    }

    void uninitializeBackend() noexcept
    {
        for (auto &group : groups)
        {
            if (group && engine)
            {
                ma_sound_group_uninit(group.get());
            }
            group.reset();
        }
        if (engine)
        {
            ma_engine_uninit(engine.get());
            engine.reset();
        }
        available = false;
    }

    ma_sound_group *groupFor(AudioBus bus) const noexcept
    {
        if (bus == AudioBus::Master)
        {
            return nullptr;
        }
        return groups[groupIndex(bus)].get();
    }

    void applyBusVolume(AudioBus bus)
    {
        const auto &busState = buses[busIndex(bus)];
        const float effectiveVolume = busState.muted ? 0.0f : busState.volume;
        if (!available)
        {
            return;
        }
        if (bus == AudioBus::Master)
        {
            ma_engine_set_volume(engine.get(), effectiveVolume);
        }
        else
        {
            ma_sound_group_set_volume(groupFor(bus), effectiveVolume);
        }
    }

    void applyAllBusVolumes()
    {
        for (std::size_t index = 0; index < kBusCount; ++index)
        {
            applyBusVolume(static_cast<AudioBus>(index));
        }
    }

    void applyListener()
    {
        if (!available)
        {
            return;
        }
        ma_engine_listener_set_position(engine.get(), 0, listener.positionX,
            listener.positionY, listener.positionZ);
        ma_engine_listener_set_direction(engine.get(), 0, listener.forwardX,
            listener.forwardY, listener.forwardZ);
        ma_engine_listener_set_world_up(engine.get(), 0, listener.upX,
            listener.upY, listener.upZ);
    }

    std::size_t allocateVoice()
    {
        for (std::size_t index = 0; index < voices.size(); ++index)
        {
            if (!voices[index].used)
            {
                auto &slot = voices[index];
                slot.generation = slot.generation == std::numeric_limits<std::uint32_t>::max()
                    ? 1
                    : slot.generation + 1;
                slot.used = true;
                ++activeVoices;
                return index;
            }
        }

        if (voices.size() >= config.maxVoices)
        {
            return std::numeric_limits<std::size_t>::max();
        }

        voices.emplace_back();
        auto &slot = voices.back();
        slot.generation = 1;
        slot.used = true;
        ++activeVoices;
        return voices.size() - 1;
    }

    AudioVoiceId idFor(std::size_t index) const noexcept
    {
        const auto generation = voices[index].generation;
        return (static_cast<AudioVoiceId>(generation) << 32) |
            static_cast<AudioVoiceId>(index + 1);
    }

    VoiceSlot *findVoice(AudioVoiceId id) noexcept
    {
        if (id == kInvalidAudioVoice)
        {
            return nullptr;
        }
        const auto indexValue = static_cast<std::uint32_t>(id & 0xFFFFFFFFu);
        const auto generation = static_cast<std::uint32_t>(id >> 32);
        if (indexValue == 0)
        {
            return nullptr;
        }
        const auto index = static_cast<std::size_t>(indexValue - 1);
        if (index >= voices.size())
        {
            return nullptr;
        }
        auto &slot = voices[index];
        if (!slot.used || slot.generation != generation)
        {
            return nullptr;
        }
        return &slot;
    }

    const VoiceSlot *findVoice(AudioVoiceId id) const noexcept
    {
        return const_cast<Impl *>(this)->findVoice(id);
    }

    void releaseVoice(VoiceSlot &slot) noexcept
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
        if (activeVoices > 0)
        {
            --activeVoices;
        }
    }

    void stopAll() noexcept
    {
        for (auto &slot : voices)
        {
            if (slot.used)
            {
                releaseVoice(slot);
            }
        }
    }

    AudioConfig config;
    bool available = false;
    bool nullBackend = false;
    bool globallyPaused = false;
    std::unique_ptr<ma_engine> engine;
    std::array<std::unique_ptr<ma_sound_group>, kGroupCount> groups{};
    std::array<BusState, kBusCount> buses{};
    std::unordered_map<std::string, std::shared_ptr<AudioClip>> clips;
    std::vector<VoiceSlot> voices;
    std::size_t activeVoices = 0;
    AudioListener listener{};
};

AudioSystem::AudioSystem(const AudioConfig &config) : impl_(std::make_unique<Impl>(config))
{
}

AudioSystem::~AudioSystem() = default;

AudioSystem::AudioSystem(AudioSystem &&other) noexcept = default;

AudioSystem &AudioSystem::operator=(AudioSystem &&other) noexcept = default;

bool AudioSystem::isAvailable() const noexcept
{
    return impl_->available;
}

bool AudioSystem::usingNullBackend() const noexcept
{
    return impl_->nullBackend;
}

std::shared_ptr<AudioClip> AudioSystem::loadClip(const std::filesystem::path &path,
    const AudioClipOptions &options)
{
    const auto normalized = normalizedAudioPath(path, options.maxFileBytes);
    const auto key = clipKey(normalized, options.mode);
    const auto found = impl_->clips.find(key);
    if (found != impl_->clips.end())
    {
        return found->second;
    }

    auto clip = std::shared_ptr<AudioClip>(new AudioClip(normalized, options.mode));
    impl_->clips.emplace(key, clip);
    return clip;
}

AudioVoiceId AudioSystem::play(const std::shared_ptr<const AudioClip> &clip,
    const AudioPlayOptions &options)
{
    if (!clip)
    {
        throw std::invalid_argument("Audio play requires a valid clip");
    }
    validateBus(options.bus);
    validateVolume(options.volume, "Audio volume");
    validatePitch(options.pitch);
    const std::array<float, 3> position{options.x, options.y, options.z};
    for (const float value : position)
    {
        if (!std::isfinite(value))
        {
            throw std::invalid_argument("Audio position values must be finite");
        }
    }

    const auto voiceIndex = impl_->allocateVoice();
    if (voiceIndex == std::numeric_limits<std::size_t>::max())
    {
        LOG_WARN("Audio voice limit reached; play request was ignored");
        return kInvalidAudioVoice;
    }

    auto &slot = impl_->voices[voiceIndex];
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
    slot.pausedBySystem = impl_->globallyPaused;

    if (impl_->available)
    {
        slot.sound = std::make_unique<ma_sound>();
        ma_uint32 flags = clip->loadMode() == AudioLoadMode::Streaming
            ? MA_SOUND_FLAG_STREAM
            : MA_SOUND_FLAG_DECODE;

        ma_result result = MA_ERROR;
#if defined(_WIN32)
        const auto widePath = clip->path().wstring();
        result = ma_sound_init_from_file_w(impl_->engine.get(), widePath.c_str(), flags,
            impl_->groupFor(options.bus), nullptr, slot.sound.get());
#else
        const auto utf8Path = clip->path().u8string();
        result = ma_sound_init_from_file(impl_->engine.get(), utf8Path.c_str(), flags,
            impl_->groupFor(options.bus), nullptr, slot.sound.get());
#endif
        if (result != MA_SUCCESS)
        {
            LOG_WARN("Failed to load audio clip for playback: " + clip->path().u8string() +
                ", miniaudio result " + std::to_string(static_cast<int>(result)));
            impl_->releaseVoice(slot);
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

    if (impl_->globallyPaused)
    {
        slot.state = AudioVoiceState::Paused;
        return impl_->idFor(voiceIndex);
    }

    if (slot.sound && ma_sound_start(slot.sound.get()) != MA_SUCCESS)
    {
        LOG_WARN("Failed to start audio clip: " + clip->path().u8string());
        impl_->releaseVoice(slot);
        return kInvalidAudioVoice;
    }
    slot.state = AudioVoiceState::Playing;
    return impl_->idFor(voiceIndex);
}

void AudioSystem::stop(AudioVoiceId voice) noexcept
{
    if (auto *slot = impl_->findVoice(voice))
    {
        impl_->releaseVoice(*slot);
    }
}

void AudioSystem::pause(AudioVoiceId voice) noexcept
{
    if (auto *slot = impl_->findVoice(voice); slot && slot->state == AudioVoiceState::Playing)
    {
        if (slot->sound)
        {
            ma_sound_stop(slot->sound.get());
        }
        slot->manuallyPaused = true;
        slot->state = AudioVoiceState::Paused;
    }
}

void AudioSystem::resume(AudioVoiceId voice) noexcept
{
    if (auto *slot = impl_->findVoice(voice); slot && slot->state == AudioVoiceState::Paused)
    {
        slot->manuallyPaused = false;
        if (impl_->globallyPaused)
        {
            slot->pausedBySystem = true;
            return;
        }
        if (slot->sound && ma_sound_start(slot->sound.get()) != MA_SUCCESS)
        {
            impl_->releaseVoice(*slot);
            return;
        }
        slot->pausedBySystem = false;
        slot->state = AudioVoiceState::Playing;
    }
}

void AudioSystem::setVoiceVolume(AudioVoiceId voice, float volume)
{
    validateVolume(volume, "Audio voice volume");
    if (auto *slot = impl_->findVoice(voice))
    {
        slot->volume = volume;
        if (slot->sound)
        {
            ma_sound_set_volume(slot->sound.get(), volume);
        }
    }
}

void AudioSystem::setVoicePitch(AudioVoiceId voice, float pitch)
{
    validatePitch(pitch);
    if (auto *slot = impl_->findVoice(voice))
    {
        slot->pitch = pitch;
        if (slot->sound)
        {
            ma_sound_set_pitch(slot->sound.get(), pitch);
        }
    }
}

void AudioSystem::setVoiceLooping(AudioVoiceId voice, bool looping) noexcept
{
    if (auto *slot = impl_->findVoice(voice))
    {
        slot->looping = looping;
        if (slot->sound)
        {
            ma_sound_set_looping(slot->sound.get(), looping ? MA_TRUE : MA_FALSE);
        }
    }
}

void AudioSystem::setVoiceSpatial(AudioVoiceId voice, bool spatial) noexcept
{
    if (auto *slot = impl_->findVoice(voice))
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

void AudioSystem::setVoicePosition(AudioVoiceId voice, float x, float y, float z) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    {
        return;
    }
    if (auto *slot = impl_->findVoice(voice))
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

bool AudioSystem::isPlaying(AudioVoiceId voice) const noexcept
{
    const auto *slot = impl_->findVoice(voice);
    if (!slot || slot->state != AudioVoiceState::Playing)
    {
        return false;
    }
    return !slot->sound || ma_sound_is_playing(slot->sound.get()) == MA_TRUE;
}

AudioVoiceState AudioSystem::state(AudioVoiceId voice) const noexcept
{
    const auto *slot = impl_->findVoice(voice);
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

void AudioSystem::stopAll() noexcept
{
    impl_->stopAll();
}

void AudioSystem::setBusVolume(AudioBus bus, float volume)
{
    validateBus(bus);
    validateVolume(volume, "Audio bus volume");
    impl_->buses[busIndex(bus)].volume = volume;
    impl_->applyBusVolume(bus);
}

float AudioSystem::busVolume(AudioBus bus) const noexcept
{
    if (busIndex(bus) >= kBusCount)
    {
        return 0.0f;
    }
    return impl_->buses[busIndex(bus)].volume;
}

void AudioSystem::setBusMuted(AudioBus bus, bool muted)
{
    validateBus(bus);
    impl_->buses[busIndex(bus)].muted = muted;
    impl_->applyBusVolume(bus);
}

bool AudioSystem::isBusMuted(AudioBus bus) const noexcept
{
    if (busIndex(bus) >= kBusCount)
    {
        return false;
    }
    return impl_->buses[busIndex(bus)].muted;
}

void AudioSystem::update(float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
    {
        throw std::invalid_argument("Audio delta time must be finite and non-negative");
    }
    if (!impl_->available)
    {
        return;
    }

    for (auto &slot : impl_->voices)
    {
        if (slot.used && slot.sound && !slot.looping && ma_sound_at_end(slot.sound.get()) == MA_TRUE)
        {
            impl_->releaseVoice(slot);
        }
    }
}

void AudioSystem::setListener(const AudioListener &listener)
{
    validateListener(listener);
    impl_->listener = listener;
    impl_->applyListener();
}

const AudioListener &AudioSystem::listener() const noexcept
{
    return impl_->listener;
}

void AudioSystem::setPaused(bool paused) noexcept
{
    if (impl_->globallyPaused == paused)
    {
        return;
    }
    impl_->globallyPaused = paused;
    for (auto &slot : impl_->voices)
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
                impl_->releaseVoice(slot);
                continue;
            }
            slot.pausedBySystem = false;
            slot.state = AudioVoiceState::Playing;
        }
    }
}

bool AudioSystem::isPaused() const noexcept
{
    return impl_->globallyPaused;
}

std::size_t AudioSystem::activeVoiceCount() const noexcept
{
    return impl_->activeVoices;
}

std::size_t AudioSystem::clipCount() const noexcept
{
    return impl_->clips.size();
}
