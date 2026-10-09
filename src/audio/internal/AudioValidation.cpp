#include "AudioValidation.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <system_error>

namespace AudioValidation
{
    std::size_t busIndex(AudioBus bus) noexcept
    {
        return static_cast<std::size_t>(bus);
    }

    std::size_t groupIndex(AudioBus bus)
    {
        if (bus == AudioBus::Master)
        {
            throw std::invalid_argument("Master bus does not have a sound group");
        }
        return busIndex(bus) - 1;
    }

    void validateBus(AudioBus bus)
    {
        if (busIndex(bus) >= kBusCount)
        {
            throw std::invalid_argument("Invalid audio bus");
        }
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

    AudioConfig checkedConfig(const AudioConfig &config)
    {
        if (config.maxVoices == 0)
        {
            throw std::invalid_argument("Audio maxVoices must be positive");
        }
        return config;
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
