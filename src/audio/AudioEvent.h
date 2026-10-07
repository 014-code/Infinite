#pragma once

#include "AudioSystem.h"

#include <cstddef>
#include <memory>
#include <vector>

class MusicPlayer;

enum class AudioEventType
{
    PlayOneShot,
    PlayMusic,
    StopMusic,
    StopAll
};

// 音频事件是应用层/动画层与AudioSystem之间的窄数据接口。
// 事件不保存裸指针，Clip使用shared_ptr保证排队期间资源仍然有效。
struct AudioEvent
{
    AudioEventType type = AudioEventType::PlayOneShot;
    std::shared_ptr<const AudioClip> clip;
    AudioPlayOptions options;
    float fadeSeconds = 0.0f;
};

class AudioEventQueue final
{
public:
    void push(AudioEvent event);
    void playOneShot(std::shared_ptr<const AudioClip> clip,
        const AudioPlayOptions &options = {});
    void playMusic(std::shared_ptr<const AudioClip> clip,
        const AudioPlayOptions &options = {}, float fadeSeconds = 0.0f);
    void stopMusic(float fadeSeconds = 0.0f);
    void stopAll();

    std::size_t size() const noexcept { return events_.size(); }
    void dispatch(AudioSystem &audio, MusicPlayer &music);

private:
    std::vector<AudioEvent> events_;
};
