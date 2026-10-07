#include "AudioEvent.h"

#include "MusicPlayer.h"

#include <utility>

void AudioEventQueue::push(AudioEvent event)
{
    events_.push_back(std::move(event));
}

void AudioEventQueue::playOneShot(std::shared_ptr<const AudioClip> clip,
    const AudioPlayOptions &options)
{
    push({AudioEventType::PlayOneShot, std::move(clip), options, 0.0f});
}

void AudioEventQueue::playMusic(std::shared_ptr<const AudioClip> clip,
    const AudioPlayOptions &options, float fadeSeconds)
{
    push({AudioEventType::PlayMusic, std::move(clip), options, fadeSeconds});
}

void AudioEventQueue::stopMusic(float fadeSeconds)
{
    push({AudioEventType::StopMusic, {}, {}, fadeSeconds});
}

void AudioEventQueue::stopAll()
{
    push({AudioEventType::StopAll, {}, {}, 0.0f});
}

void AudioEventQueue::dispatch(AudioSystem &audio, MusicPlayer &music)
{
    std::vector<AudioEvent> events;
    events.swap(events_);
    for (const auto &event : events)
    {
        switch (event.type)
        {
        case AudioEventType::PlayOneShot:
            audio.play(event.clip, event.options);
            break;
        case AudioEventType::PlayMusic:
            music.play(event.clip, event.options, event.fadeSeconds);
            break;
        case AudioEventType::StopMusic:
            music.stop(event.fadeSeconds);
            break;
        case AudioEventType::StopAll:
            music.stop();
            audio.stopAll();
            break;
        }
    }
}
