#pragma once

#include <glm/vec4.hpp>
#include <memory>
#include <array>
#include <optional>
#include "graphics/materials/PbrParameters.h"

class Shader;
class Texture;

// 显式选择渲染方式，不根据颜色Alpha猜测：纹理本身也可能带透明通道。
enum class RenderMode
{
    Opaque,
    AlphaBlend,
    AlphaMask // 硬裁剪：参与不透明组并写深度，由PBR Shader按alphaCutoff丢弃片段。
};

enum class CullMode
{
    None,
    Back
};

class Material
{
public:
    Material(const Material &) = default;
    Material &operator=(const Material &) = default;

    // 组合一个Shader、一个可选Texture和材质颜色。
    // 此入口只保存引用，不拥有这些资源的生命周期；被引用对象不得提前销毁或移动。
    explicit Material(
        Shader &shader,
        const glm::vec4 &baseColor = glm::vec4(1.0f),
        Texture *texture = nullptr);

    // 持有模式：与其他材质共享Shader/Texture，最后一个持有者释放资源。
    // 不负责资源查找或缓存；所有GPU资源仍须在Window析构之前释放。
    explicit Material(std::shared_ptr<Shader> shader,
        const glm::vec4 &baseColor = glm::vec4(1.0f), std::shared_ptr<Texture> texture = {});

    // 绑定Shader/纹理并上传材质uniform；不切换深度、混合或剔除，这些由Renderer管理。
    void use() const;

    // 只上传当前材质的uniform并绑定纹理，不调用Shader::use。
    // Renderer在同一批次内已经确认目标Shader处于当前程序时使用它，避免相同材质
    // 的多个物体重复切换程序；普通调用方应继续使用上面的use()完整入口。
    void applyParameters() const;

    // 创建一个材质实例：颜色、渲染模式、剔除和PBR参数独立复制，Shader/Texture继续共享。
    // 这是修改ResourceManager缓存材质前的推荐入口，避免一个物体的运行时改动污染其他物体。
    // 克隆不会创建新的OpenGL资源，返回的Material仍须在有效OpenGL上下文销毁。
    std::shared_ptr<Material> clone() const;

    // 修改材质颜色。Shader需要包含名为baseColor的vec4 uniform。
    void setBaseColor(const glm::vec4 &baseColor);
    const glm::vec4 &baseColor() const;
    // nullptr/空shared_ptr表示无纹理；无纹理时清除0号绑定，避免采样前一个材质。
    // Shader可以使用hasTexture(bool)决定是否采样；也可以使用纯颜色Shader。
    void setTexture(Texture *texture);
    void setTexture(std::shared_ptr<Texture> texture);
    const Texture *texture() const;

    // 这些配置由Renderer::drawItems使用；低层draw仍沿用调用者的OpenGL状态。
    void setRenderMode(RenderMode mode);
    RenderMode renderMode() const;
    void setCullMode(CullMode mode);
    CullMode cullMode() const;

    // 默认保留旧示例的固定逆时针规则；导入模型可开启镜像修正，
    // Renderer按最终世界矩阵决定正面绕序，包含祖先节点的负缩放。
    void setCorrectMirroredWinding(bool enabled) noexcept;
    bool correctMirroredWinding() const noexcept;
    // Shader自己编码sRGB输出时，Renderer临时禁用帧缓冲sRGB转换，避免二次编码。
    void setShaderOutputsSrgb(bool enabled) noexcept;
    bool shaderOutputsSrgb() const noexcept;

    // 获取材质使用的Shader，供Renderer上传通用矩阵uniform。
    const Shader &shader() const;

    // 显式启用线性PBR路径，不通过Shader文件名猜测。Shader须符合内置PBR接口。
    void setPbrParameters(const PbrParameters &parameters);
    const std::optional<PbrParameters> &pbrParameters() const noexcept { return pbr_; }
    void setPbrTexture(PbrTextureSlot slot, std::shared_ptr<Texture> texture);
    const Texture *pbrTexture(PbrTextureSlot slot) const;

private:
    std::shared_ptr<Shader> ownedShader_;
    std::shared_ptr<Texture> ownedTexture_;
    Shader *shader_ = nullptr;
    Texture *texture_ = nullptr;
    glm::vec4 baseColor_{1.0f};
    RenderMode renderMode_ = RenderMode::Opaque;
    CullMode cullMode_ = CullMode::None;
    bool correctMirroredWinding_ = false;
    bool shaderOutputsSrgb_ = false;
    std::optional<PbrParameters> pbr_;
    std::array<std::shared_ptr<Texture>, 4> pbrTextures_;
};
