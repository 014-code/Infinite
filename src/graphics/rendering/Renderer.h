#pragma once

#include "graphics/rendering/RenderItem.h"

#include <cstddef>

class Mesh;
class Material;
class Camera;
class Transform;
struct DirectionalLight;
class SceneLighting;
struct DirectionalShadowView;

// Renderer只统计本次drawItems调用实际提交的绘制，便于示例和性能测试观察批次变化。
// 这些数据是诊断信息，不参与渲染结果，也不代表GPU已经完成执行。
struct RenderStats
{
    std::size_t drawCalls = 0;
    std::size_t shaderChanges = 0;
    std::size_t materialChanges = 0;
    std::size_t meshChanges = 0;
    std::size_t opaqueItems = 0;
    std::size_t transparentItems = 0;
};

class Renderer
{
public:
    // 先绘制不透明组，再按视空间深度绘制透明组；每个物体应用材质的剔除方式。
    // 自动恢复深度/混合/剔除状态和程序、VAO、纹理绑定；不恢复Shader的uniform值。
    // 不清屏、不切换帧缓冲。空列表不操作OpenGL；缺少必需指针时在绘制前抛异常。
    // 不能检测悬空指针，调用者仍须保证所有借用资源在绘制期间有效。
    // 相机矩阵和逐物体矩阵只在本批次内复用；下一次调用重新计算，不缓存跨帧Transform状态。
    void drawItems(const std::vector<RenderItem> &items, const Camera &camera, float aspectRatio) const;

    // 使用指定方向光绘制；旧接口保留默认方向光，兼容不需要光照参数的示例。
    void drawItems(const std::vector<RenderItem> &items, const Camera &camera,
        float aspectRatio, const DirectionalLight &light) const;
    // 场景多光源入口；一次性校验所有光源及Shader能力后才修改OpenGL状态。
    void drawItems(const std::vector<RenderItem> &items, const Camera &camera,
        float aspectRatio, const SceneLighting &lighting, const DirectionalShadowView *shadow = nullptr,
        bool requireLinearOutput = false) const;

    // 返回最近一次drawItems的统计快照；调用者只能读取，不应把它当成同步GPU计时结果。
    const RenderStats &lastStats() const noexcept { return lastStats_; }

    // 清空颜色和深度缓冲。临时开启深度写入以完成清理，随后恢复原写入开关。
    void clear(float red, float green, float blue, float alpha) const;

    // 开启或关闭深度测试。默认由OpenGL保持关闭状态。
    void setDepthTestEnabled(bool enabled) const;

    // 设置深度写入。透明物体通常只测试深度、不写入深度，避免挡住其他透明层。
    // 此状态会一直保留到下一次调用；透明绘制结束后应重新设为true。
    void setDepthWriteEnabled(bool enabled) const;

    // 开启常规非预乘Alpha混合：颜色 = 前景*Alpha + 背景*(1-Alpha)。
    // 不会自动处理物体排序，也不会修改深度测试或深度写入状态。
    void setAlphaBlendingEnabled(bool enabled) const;

    // 开启背面剔除：投影到屏幕后，逆时针排列的三角形视为正面。
    // 关闭后两面都可绘制。此开关修改当前OpenGL上下文状态，不属于单个Mesh。
    // 必须在窗口上下文创建后调用；不会改变深度测试或其他渲染状态。
    void setFaceCullingEnabled(bool enabled) const;

    // 低层单物体绘制：使用材质、摄像机和物体变换，不自动分组或修改深度/混合/剔除。
    // 保留旧接口的手动状态行为；需要应用Material渲染配置时使用drawItems或Scene。
    void draw(
        const Mesh &mesh,
        const Material &material,
        const Transform &transform,
        const Camera &camera,
        float aspectRatio) const;

    void draw(
        const Mesh &mesh,
        const Material &material,
        const Transform &transform,
        const Camera &camera,
        float aspectRatio,
        const DirectionalLight &light) const;

    void draw(const Mesh &mesh, const Material &material, const Transform &transform,
        const Camera &camera, float aspectRatio, const SceneLighting &lighting) const;

private:
    mutable RenderStats lastStats_;
};
