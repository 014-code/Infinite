#include "AudioDeviceBackend.h"

#include "audio/internal/AudioValidation.h"
#include "core/Log.h"

#include <string>

AudioDeviceBackend::AudioDeviceBackend(const AudioConfig &config)
{
    if (config.enabled)
    {
        initialize(config);
    }
    else
    {
        nullBackend_ = true;
    }
}

AudioDeviceBackend::~AudioDeviceBackend()
{
    uninitialize();
}

void AudioDeviceBackend::initialize(const AudioConfig &config)
{
    engine_ = std::make_unique<ma_engine>();
    ma_engine_config engineConfig = ma_engine_config_init();
    const ma_result engineResult = ma_engine_init(&engineConfig, engine_.get());
    if (engineResult != MA_SUCCESS)
    {
        engine_.reset();
        handleFailure(config, "audio device initialization", engineResult);
        return;
    }

    for (std::size_t index = 0; index < AudioValidation::kGroupCount; ++index)
    {
        auto group = std::make_unique<ma_sound_group>();
        const ma_result groupResult = ma_sound_group_init(
            engine_.get(), 0, nullptr, group.get());
        if (groupResult != MA_SUCCESS)
        {
            uninitialize();
            handleFailure(config, "audio bus initialization", groupResult);
            return;
        }
        groups_[index] = std::move(group);
    }

    available_ = true;
    nullBackend_ = false;
}

void AudioDeviceBackend::handleFailure(const AudioConfig &config, const char *operation,
    ma_result result)
{
    const std::string message = std::string(operation) + " failed with miniaudio result " +
        std::to_string(static_cast<int>(result));
    if (!config.allowNullBackend)
    {
        throw std::runtime_error(message);
    }
    LOG_WARN(message + "; using the null audio backend");
    available_ = false;
    nullBackend_ = true;
}

void AudioDeviceBackend::uninitialize() noexcept
{
    for (auto &group : groups_)
    {
        if (group && engine_)
        {
            ma_sound_group_uninit(group.get());
        }
        group.reset();
    }
    if (engine_)
    {
        ma_engine_uninit(engine_.get());
        engine_.reset();
    }
    available_ = false;
}

ma_sound_group *AudioDeviceBackend::groupFor(AudioBus bus) const noexcept
{
    if (bus == AudioBus::Master)
    {
        return nullptr;
    }
    // 公开AudioSystem已在进入后端前校验bus；这里使用无异常索引，
    // 保证这个仅用于内部有效配置的函数可以继续满足noexcept契约。
    return groups_[AudioValidation::busIndex(bus) - 1].get();
}

void AudioDeviceBackend::setBusVolume(AudioBus bus, float volume) noexcept
{
    if (!available_)
    {
        return;
    }
    if (bus == AudioBus::Master)
    {
        ma_engine_set_volume(engine_.get(), volume);
    }
    else
    {
        ma_sound_group_set_volume(groupFor(bus), volume);
    }
}

void AudioDeviceBackend::setListener(const AudioListener &listener) noexcept
{
    if (!available_)
    {
        return;
    }
    ma_engine_listener_set_position(engine_.get(), 0, listener.positionX,
        listener.positionY, listener.positionZ);
    ma_engine_listener_set_direction(engine_.get(), 0, listener.forwardX,
        listener.forwardY, listener.forwardZ);
    ma_engine_listener_set_world_up(engine_.get(), 0, listener.upX,
        listener.upY, listener.upZ);
}
