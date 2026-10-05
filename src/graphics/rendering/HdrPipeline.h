#pragma once

#include <glm/vec4.hpp>
#include <functional>
#include <memory>

// 线性RGBA16F场景目标 + 一次最终输出。不包含物体/摄像机/灯光，借回调接入应用。
// 构造不调用GL，首次render按需分配；仅在有效上下文主线程使用，早于Window销毁。
class HdrPipeline final
{
public:
    HdrPipeline();
    ~HdrPipeline();
    HdrPipeline(const HdrPipeline &) = delete;
    HdrPipeline &operator=(const HdrPipeline &) = delete;
    // draw内只提交线性输出的材质。输出到调用前的draw FBO/viewport；异常也恢复被改动状态。
    // 暂不支持递归调用、MSAA、Bloom或IBL。exposure须为有限正数；toneMapping为Reinhard开关。
    void render(int width, int height, const glm::vec4 &clearColor,
        const std::function<void()> &draw, float exposure = 1, bool toneMapping = true);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
