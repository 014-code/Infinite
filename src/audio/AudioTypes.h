#pragma once

#include <cstddef>
#include <cstdint>

// 音频文件的加载策略。
//
// Static适合短音效：播放前解码，播放时延迟较低。
// Streaming适合较长的背景音乐：由后端按播放进度读取，避免一次性占用大量内存。
enum class AudioLoadMode
{
    Static,
    Streaming
};

// 音频混音总线。所有Voice最终都会进入其中一个非Master总线，
// 再统一经过Master总线输出，方便实现“音乐音量”和“音效音量”等设置。
enum class AudioBus
{
    Master,
    Music,
    Sfx,
    Ui,
    Ambient
};

// 音频设备初始化配置。
struct AudioConfig
{
    // 关闭后仍保留Null Backend行为，适合不需要声音的工具和自动化测试。
    bool enabled = true;
    // 设备打开失败时是否降级为静音后端。游戏一般应保持true，避免没有声卡时无法启动。
    bool allowNullBackend = true;
    // 同时存在的播放实例上限。超过上限时新播放请求返回无效句柄，并记录诊断信息。
    std::size_t maxVoices = 64;
};

// 加载一份音频资源时使用的选项。
struct AudioClipOptions
{
    AudioLoadMode mode = AudioLoadMode::Static;
    // 防止损坏文件或错误路径在静态解码前占用过多磁盘/内存资源。
    std::size_t maxFileBytes = 256 * 1024 * 1024;
};

// 一次播放实例的参数。AudioClip是共享资源，AudioPlayOptions只影响本次播放。
struct AudioPlayOptions
{
    AudioBus bus = AudioBus::Sfx;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool looping = false;
    bool spatial = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// 场景中的听觉位置。第一阶段只保留一个Listener，后续如有需要再扩展多Listener。
struct AudioListener
{
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float forwardX = 0.0f;
    float forwardY = 0.0f;
    float forwardZ = -1.0f;
    float upX = 0.0f;
    float upY = 1.0f;
    float upZ = 0.0f;
};
