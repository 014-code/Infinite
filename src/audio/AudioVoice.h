#pragma once

#include <cstdint>

// 播放实例使用索引+代数编码的句柄，而不是把后端指针暴露给调用方。
// 旧句柄即使对应的槽位后来被复用，也不会误操作新的声音。
using AudioVoiceId = std::uint64_t;
constexpr AudioVoiceId kInvalidAudioVoice = 0;

enum class AudioVoiceState
{
    Invalid,
    Playing,
    Paused,
    Stopped
};
