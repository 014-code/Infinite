#pragma once

class Scene;

// SceneAudioSync只负责把场景物体的空间位置同步到音频组件。
// 音频设备、Voice生命周期和播放控制仍归AudioSystem/AudioSourceComponent所有。
class SceneAudioSync final
{
public:
    // 只同步启用物体；非空间音源会在组件内部快速返回。
    static void sync(const Scene &scene);
};
