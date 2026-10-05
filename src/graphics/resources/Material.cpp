#include "graphics/resources/Material.h"

#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"

#include <cmath>
#include <stdexcept>
#include <utility>

Material::Material(
    Shader &shader,
    const glm::vec4 &baseColor,
    Texture *texture)
    : shader_(&shader), texture_(texture), baseColor_(baseColor)
{
    setBaseColor(baseColor);
}

Material::Material(std::shared_ptr<Shader> shader, const glm::vec4 &baseColor,
    std::shared_ptr<Texture> texture)
    : ownedShader_(std::move(shader)), ownedTexture_(std::move(texture)),
      shader_(ownedShader_.get()), texture_(ownedTexture_.get())
{
    if (shader_ == nullptr) { throw std::invalid_argument("Material requires a Shader"); }
    setBaseColor(baseColor);
}

void Material::use() const
{
    // 先激活材质使用的Shader，再上传材质参数。
    shader_->use();
    shader_->setVec4("baseColor", baseColor_);
    shader_->setInt("hasTexture", texture_ != nullptr ? 1 : 0);

    // 有纹理时绑定0号纹理单元，并设置对应采样器。
    if (texture_ != nullptr)
    {
        texture_->bind(0);
        shader_->setInt("textureSampler", 0);
    }
    else
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (pbr_)
    {
        shader_->setFloat("metallicFactor", pbr_->metallic);
        shader_->setFloat("roughnessFactor", pbr_->roughness);
        shader_->setVec3("emissionFactor", pbr_->emission);
        shader_->setFloat("normalScale", pbr_->normalScale);
        shader_->setFloat("occlusionStrength", pbr_->occlusionStrength);
        shader_->setFloat("alphaCutoff", pbr_->alphaCutoff);
        shader_->setInt("alphaMode", renderMode_ == RenderMode::AlphaMask ? 1 :
            renderMode_ == RenderMode::AlphaBlend ? 2 : 0);
        shader_->setInt("unlit", pbr_->unlit);
        const char *samplers[] = {"metallicRoughnessTexture", "normalTexture", "occlusionTexture", "emissionTexture"};
        const char *flags[] = {"hasMetallicRoughness", "hasNormalTexture", "hasOcclusion", "hasEmission"};
        for (std::size_t i = 0; i < pbrTextures_.size(); ++i)
        {
            glActiveTexture(GL_TEXTURE1 + static_cast<GLenum>(i));
            if (pbrTextures_[i]) { pbrTextures_[i]->bind(static_cast<GLuint>(i + 1)); }
            else { glBindTexture(GL_TEXTURE_2D, 0); }
            shader_->setInt(samplers[i], static_cast<int>(i + 1));
            shader_->setInt(flags[i], pbrTextures_[i] != nullptr);
        }
    }
}

void Material::setBaseColor(const glm::vec4 &baseColor)
{
    for (int component = 0; component < 4; ++component)
    {
        if (!std::isfinite(baseColor[component])) { throw std::invalid_argument("Material color must be finite"); }
    }
    if (baseColor.a < 0 || baseColor.a > 1) { throw std::invalid_argument("Material alpha must be in [0, 1]"); }
    baseColor_ = baseColor;
}

const glm::vec4 &Material::baseColor() const { return baseColor_; }
const Texture *Material::texture() const { return texture_; }

void Material::setTexture(Texture *texture)
{
    // 这个重载是借用模式：调用者负责保证texture在Material使用期间仍然存活。
    // 若此前是持有模式且换成了不同指针，必须释放旧shared_ptr，避免同时保留两种所有权语义。
    // 同一指针重复绑定不能意外释放自己唯一持有的纹理。
    if (texture != texture_) { ownedTexture_.reset(); }
    texture_ = texture;
}

void Material::setTexture(std::shared_ptr<Texture> texture)
{
    // 这个重载是持有模式；空shared_ptr表示清除纹理，但不会清除Shader。
    ownedTexture_ = std::move(texture);
    texture_ = ownedTexture_.get();
}

const Shader &Material::shader() const
{
    return *shader_;
}

void Material::setRenderMode(RenderMode mode)
{
    if (mode != RenderMode::Opaque && mode != RenderMode::AlphaBlend && mode != RenderMode::AlphaMask) { throw std::invalid_argument("Invalid render mode"); }
    renderMode_ = mode;
}

RenderMode Material::renderMode() const
{
    return renderMode_;
}

void Material::setCullMode(CullMode mode)
{
    if (mode != CullMode::None && mode != CullMode::Back) { throw std::invalid_argument("Invalid cull mode"); }
    cullMode_ = mode;
}

CullMode Material::cullMode() const
{
    return cullMode_;
}

void Material::setCorrectMirroredWinding(bool enabled) noexcept { correctMirroredWinding_ = enabled; }
bool Material::correctMirroredWinding() const noexcept { return correctMirroredWinding_; }
void Material::setShaderOutputsSrgb(bool enabled) noexcept { shaderOutputsSrgb_ = enabled; }
bool Material::shaderOutputsSrgb() const noexcept { return shaderOutputsSrgb_; }

void Material::setPbrParameters(const PbrParameters &parameters)
{
    parameters.validate();
    pbr_ = parameters;
    shaderOutputsSrgb_ = false; // PBR输出线性HDR；最终编码由输出阶段统一负责。
}

void Material::setPbrTexture(PbrTextureSlot slot, std::shared_ptr<Texture> texture)
{
    const auto index = static_cast<std::size_t>(slot);
    if (index >= pbrTextures_.size()) { throw std::invalid_argument("Invalid PBR texture slot"); }
    if (texture && texture->isSrgb() != (slot == PbrTextureSlot::Emission))
    {
        throw std::invalid_argument("PBR emission uses sRGB; normal/MR/AO textures use linear data");
    }
    pbrTextures_[index] = std::move(texture);
}

const Texture *Material::pbrTexture(PbrTextureSlot slot) const
{
    const auto index = static_cast<std::size_t>(slot);
    if (index >= pbrTextures_.size()) { throw std::invalid_argument("Invalid PBR texture slot"); }
    return pbrTextures_[index].get();
}
