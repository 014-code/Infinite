#include "TestSupport.h"
#include "audio/AudioSystem.h"
#include "audio/AudioEvent.h"
#include "audio/MusicPlayer.h"
#include "scene/Scene.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    void writeLittleEndian(std::ofstream &file, std::uint32_t value)
    {
        const std::array<char, 4> bytes{
            static_cast<char>(value & 0xFFu),
            static_cast<char>((value >> 8) & 0xFFu),
            static_cast<char>((value >> 16) & 0xFFu),
            static_cast<char>((value >> 24) & 0xFFu)};
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    void writeLittleEndian16(std::ofstream &file, std::uint16_t value)
    {
        const std::array<char, 2> bytes{
            static_cast<char>(value & 0xFFu),
            static_cast<char>((value >> 8) & 0xFFu)};
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    // 写入一份极短的PCM WAV，既能在真实后端播放，也不需要把二进制资源提交到仓库。
    void writeSilentWav(const std::filesystem::path &path)
    {
        constexpr std::uint32_t sampleRate = 8000;
        constexpr std::uint32_t sampleCount = 800;
        constexpr std::uint32_t dataSize = sampleCount;
        constexpr std::uint32_t riffSize = 36 + dataSize;
        std::ofstream file(path, std::ios::binary);
        require(file.good(), "Cannot create temporary WAV file");
        file.write("RIFF", 4);
        writeLittleEndian(file, riffSize);
        file.write("WAVEfmt ", 8);
        writeLittleEndian(file, 16);
        writeLittleEndian16(file, 1); // PCM
        writeLittleEndian16(file, 1); // mono
        writeLittleEndian(file, sampleRate);
        writeLittleEndian(file, sampleRate); // byte rate: 8000 * 1 * 1
        writeLittleEndian16(file, 1); // block align
        writeLittleEndian16(file, 8); // bits per sample
        file.write("data", 4);
        writeLittleEndian(file, dataSize);
        const std::vector<char> silence(sampleCount, static_cast<char>(128));
        file.write(silence.data(), static_cast<std::streamsize>(silence.size()));
        require(file.good(), "Cannot write temporary WAV file");
    }
}

int main()
{
    try
    {
        const auto path = std::filesystem::temp_directory_path() / "infinite_audio_test.wav";
        std::filesystem::remove(path);
        writeSilentWav(path);

        AudioConfig config;
        config.enabled = false;
        config.maxVoices = 2;
        AudioSystem audio(config);
        require(!audio.isAvailable() && audio.usingNullBackend(),
            "Disabled audio did not use the null backend");

        auto staticClip = audio.loadClip(path);
        auto sameStaticClip = audio.loadClip(path);
        require(staticClip == sameStaticClip, "Audio clip cache missed an identical request");
        AudioClipOptions streamingOptions;
        streamingOptions.mode = AudioLoadMode::Streaming;
        auto streamingClip = audio.loadClip(path, streamingOptions);
        require(staticClip != streamingClip, "Audio load mode was not part of the cache key");
        require(audio.clipCount() == 2, "Unexpected audio clip cache count");

        AudioPlayOptions options;
        options.bus = AudioBus::Sfx;
        const AudioVoiceId voice = audio.play(staticClip, options);
        require(voice != kInvalidAudioVoice, "Null backend did not return a voice handle");
        require(audio.activeVoiceCount() == 1 && audio.isPlaying(voice),
            "Audio voice did not enter the playing state");
        audio.pause(voice);
        require(audio.state(voice) == AudioVoiceState::Paused, "Audio voice did not pause");
        audio.resume(voice);
        require(audio.isPlaying(voice), "Audio voice did not resume");

        audio.setBusVolume(AudioBus::Music, 0.35f);
        require(audio.busVolume(AudioBus::Music) == 0.35f, "Audio bus volume was not stored");
        audio.setBusMuted(AudioBus::Music, true);
        require(audio.isBusMuted(AudioBus::Music), "Audio bus mute was not stored");
        audio.setBusMuted(AudioBus::Music, false);

        AudioListener listener;
        listener.positionX = 2.0f;
        listener.forwardZ = -0.5f;
        audio.setListener(listener);
        require(audio.listener().positionX == 2.0f, "Audio listener was not stored");

        audio.setPaused(true);
        require(audio.isPaused() && audio.state(voice) == AudioVoiceState::Paused,
            "Global audio pause did not pause active voices");
        audio.setPaused(false);
        require(!audio.isPaused() && audio.isPlaying(voice),
            "Global audio resume did not resume active voices");

        audio.stop(voice);
        require(audio.state(voice) == AudioVoiceState::Invalid && audio.activeVoiceCount() == 0,
            "Stopped audio voice remained valid");
        audio.stop(voice); // 旧句柄重复停止必须安全。

        // MusicPlayer在AudioSystem之上管理循环音乐的当前Voice和交叉淡入淡出。
        MusicPlayer music(audio);
        music.play(staticClip);
        require(music.isPlaying(), "MusicPlayer did not start music");
        music.play(streamingClip, {}, 1.0f);
        require(audio.activeVoiceCount() == 2, "Music crossfade did not keep both voices");
        music.update(0.5f);
        music.update(0.5f);
        require(audio.activeVoiceCount() == 1 && music.isPlaying(),
            "Music crossfade did not finish cleanly");
        music.stop(0.25f);
        music.update(0.25f);
        require(!music.isPlaying() && audio.activeVoiceCount() == 0,
            "Music fade out did not stop the voice");

        // 音频事件队列把一次性音效和音乐切换延迟到Application音频阶段执行。
        AudioEventQueue events;
        events.playOneShot(staticClip);
        events.playMusic(streamingClip, {}, 0.0f);
        require(events.size() == 2, "Audio event queue did not retain events");
        events.dispatch(audio, music);
        require(events.size() == 0 && music.isPlaying(), "Audio event queue dispatch failed");
        events.stopAll();
        events.dispatch(audio, music);
        require(audio.activeVoiceCount() == 0, "Audio stop-all event failed");

        // AudioSourceComponent只负责把持续声音挂到物体上，Scene负责每帧同步空间位置。
        Scene scene;
        auto &object = scene.createObject("audio source");
        object.setAudioSource(audio, staticClip);
        object.audioSource().setSpatial(true);
        object.audioSource().setLooping(true);
        object.transform.position = {2.0f, 1.0f, -3.0f};
        object.audioSource().play();
        require(object.audioSource().isPlaying() && audio.activeVoiceCount() == 1,
            "Audio source component did not start its voice");
        scene.syncAudio();
        object.audioSource().stop();
        object.clearAudioSource();
        require(audio.activeVoiceCount() == 0, "Audio source component did not release its voice");

        expectThrow<std::invalid_argument>([&]
        {
            AudioPlayOptions invalid;
            invalid.volume = 2.0f;
            audio.play(staticClip, invalid);
        }, "Invalid audio volume accepted");
        expectThrow<std::runtime_error>([&]
        {
            AudioClipOptions invalid;
            invalid.maxFileBytes = 1;
            audio.loadClip(path, invalid);
        }, "Audio file size limit was ignored");
        expectThrow<std::invalid_argument>([&] { audio.update(std::numeric_limits<float>::quiet_NaN()); },
            "Invalid audio delta accepted");
        expectThrow<std::runtime_error>([&] { audio.loadClip(path.parent_path() / "missing.wav"); },
            "Missing audio file accepted");

        std::filesystem::remove(path);
        std::cout << "Audio system passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
