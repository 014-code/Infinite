#include "DirectionalShadowMap.h"
#include "ShadowPassState.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Material.h"
#include "graphics/resources/Texture.h"
#include "graphics/resources/Shader.h"
#include "graphics/shaders/SkinShaderSource.h"
#include "math/Transform.h"
#include "PbrShaderSources.h"
#include <cmath>
#include <stdexcept>

namespace
{
    struct DepthTarget
    {
        GLuint fbo=0, texture=0;
        int size;
        explicit DepthTarget(int resolution) : size(resolution)
        {
            try
            {
                glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
                glGenTextures(1,&texture); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture);
                glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,size,size,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
                const float border[4]={1,1,1,1}; glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
                glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,texture,0);
                glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
                if (!fbo || !texture || glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
                { throw std::runtime_error("Failed to create shadow depth target"); }
            }
            catch (...) { release(); throw; }
        }
        ~DepthTarget() { release(); }
        void release() { glDeleteFramebuffers(1,&fbo); glDeleteTextures(1,&texture); }
    };
}

struct DirectionalShadowMap::Impl
{
    Shader shader=Shader::fromSource(configureSkinShader(PbrShaders::shadowVertex),PbrShaders::shadowFragment,"directional shadow");
    std::unique_ptr<DepthTarget> target;
};

DirectionalShadowMap::DirectionalShadowMap() = default;
DirectionalShadowMap::~DirectionalShadowMap() = default;

DirectionalShadowView DirectionalShadowMap::render(const std::vector<RenderItem> &items,
    const glm::vec3 &direction,const DirectionalShadowSettings &settings)
{
    const auto lightMatrix=settings.lightMatrix(direction);
    const ShadowPassState saved;
    GLint maximum=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    if (settings.resolution > maximum) { throw std::invalid_argument("Shadow resolution exceeds device limit"); }
    if (!impl_) { impl_=std::make_unique<Impl>(); }
    // 先验证所有投影物体；分配和绘制失败不能留下悬空目标或污染调用者GL状态。
    std::vector<glm::mat4> matrices;
    matrices.reserve(items.size());
    for (const auto &item:items)
    {
        if (!item.mesh || !item.material || !item.transform) { throw std::invalid_argument("Incomplete shadow render item"); }
        const auto matrix=item.transform->worldMatrix();
        for (int c=0;c<4;++c) { for (int r=0;r<4;++r)
        { if (!std::isfinite(matrix[c][r])) { throw std::invalid_argument("Non-finite shadow model matrix"); } } }
        matrices.push_back(matrix);
        if (!item.skinMatrices.empty() && (!item.mesh->hasSkinAttributes() ||
            item.mesh->maximumJoint()>=item.skinMatrices.size() ||
            item.skinMatrices.size()>impl_->shader.matrixArrayCapacity("jointMatrices[0]")))
        { throw std::invalid_argument("Invalid shadow skin palette"); }
        for (const auto &joint:item.skinMatrices) { for (int c=0;c<4;++c) { for (int r=0;r<4;++r)
        { if (!std::isfinite(joint[c][r])) { throw std::invalid_argument("Non-finite shadow skin matrix"); } } } }
        if (item.material->renderMode()==RenderMode::AlphaMask && !item.material->pbrParameters())
        { throw std::invalid_argument("Shadow MASK requires PBR alpha cutoff metadata"); }
    }
    if (!impl_->target || impl_->target->size!=settings.resolution)
    { impl_->target=std::make_unique<DepthTarget>(settings.resolution); }
    glBindFramebuffer(GL_FRAMEBUFFER,impl_->target->fbo);
    glViewport(0,0,settings.resolution,settings.resolution);
    for (auto capability:{GL_BLEND,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_POLYGON_OFFSET_FILL}) { glDisable(capability); }
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE); glClearDepth(1);
    glClear(GL_DEPTH_BUFFER_BIT);
    auto &shader=impl_->shader;
    shader.use(); shader.setMat4("lightMatrix",lightMatrix); shader.setInt("textureSampler",0);
    for (std::size_t index=0;index<items.size();++index)
    {
        const auto &item=items[index];
        const auto &material=*item.material;
        if (material.renderMode()==RenderMode::AlphaBlend) { continue; }
        if (material.cullMode()==CullMode::Back) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
        else { glDisable(GL_CULL_FACE); }
        glFrontFace(material.correctMirroredWinding() && glm::determinant(glm::mat3(matrices[index]))<0 ? GL_CW : GL_CCW);
        shader.setMat4("model",matrices[index]);
        shader.setInt("skinEnabled",!item.skinMatrices.empty());
        if (!item.skinMatrices.empty()) { shader.setMat4Array("jointMatrices[0]",item.skinMatrices); }
        shader.setInt("alphaMask",material.renderMode()==RenderMode::AlphaMask);
        shader.setFloat("baseAlpha",material.baseColor().a);
        shader.setFloat("alphaCutoff",material.pbrParameters() ? material.pbrParameters()->alphaCutoff : .5f);
        shader.setInt("hasTexture",material.texture()!=nullptr);
        if (material.texture()) { material.texture()->bind(0); }
        item.mesh->draw();
    }
    return {impl_->target->texture,lightMatrix,settings.bias};
}
