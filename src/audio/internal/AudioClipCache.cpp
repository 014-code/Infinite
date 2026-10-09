#include "AudioClipCache.h"

#include "audio/internal/AudioValidation.h"

std::shared_ptr<AudioClip> AudioClipCache::load(const std::filesystem::path &path,
    const AudioClipOptions &options)
{
    const auto normalized = AudioValidation::normalizedAudioPath(path, options.maxFileBytes);
    const auto key = AudioValidation::clipKey(normalized, options.mode);
    const auto found = clips_.find(key);
    if (found != clips_.end())
    {
        return found->second;
    }

    auto clip = std::shared_ptr<AudioClip>(new AudioClip(normalized, options.mode));
    clips_.emplace(key, clip);
    return clip;
}
