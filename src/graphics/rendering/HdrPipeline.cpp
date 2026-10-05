#include "HdrPipeline.h"
#include "graphics/resources/Shader.h"
#include "PbrShaderSources.h"
#include <GL/glew.h>
#include <array>
#include <cmath>
#include <stdexcept>

namespace
{
    struct State
    {
        GLint drawFbo, readFbo, program, vao, active, texture0, renderbuffer;
        std::array<GLint, 4> viewport;
        std::array<GLboolean, 4> colorMask;
        std::array<GLfloat, 4> clear;
        GLdouble clearDepth;
        GLboolean depthMask;
        const std::array<GLenum, 6> capabilities{GL_DEPTH_TEST, GL_BLEND, GL_CULL_FACE,
            GL_SCISSOR_TEST, GL_FRAMEBUFFER_SRGB, GL_STENCIL_TEST};
        std::array<GLboolean, 6> enabled;
        State()
        {
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
            glGetIntegerv(GL_CURRENT_PROGRAM, &program); glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &active); glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
            glGetIntegerv(GL_VIEWPORT, viewport.data()); glGetBooleanv(GL_COLOR_WRITEMASK, colorMask.data());
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask); glGetFloatv(GL_COLOR_CLEAR_VALUE, clear.data());
            glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clearDepth);
            glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
            glActiveTexture(active);
            for (std::size_t i = 0; i < enabled.size(); ++i) { enabled[i] = glIsEnabled(capabilities[i]); }
        }
        ~State()
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo); glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            glUseProgram(program); glBindVertexArray(vao); glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture0); glActiveTexture(active);
            glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]); glDepthMask(depthMask);
            glClearColor(clear[0], clear[1], clear[2], clear[3]); glClearDepth(clearDepth);
            for (std::size_t i = 0; i < enabled.size(); ++i)
            {
                if (enabled[i]) { glEnable(capabilities[i]); } else { glDisable(capabilities[i]); }
            }
        }
    };

    // 新尺寸先单独创建成功，才替换旧目标；失败由局部RAII回收，旧目标仍可重用。
    struct Target
    {
        GLuint fbo = 0, color = 0, depth = 0;
        int width, height;
        Target(int w, int h) : width(w), height(h)
        {
            try
            {
                glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glGenTextures(1, &color); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, color);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
                glGenRenderbuffers(1, &depth); glBindRenderbuffer(GL_RENDERBUFFER, depth);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
                if (!fbo || !color || !depth || glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                { throw std::runtime_error("Cannot create HDR color/depth target"); }
            }
            catch (...) { release(); throw; }
        }
        ~Target() { release(); }
        void release()
        {
            glDeleteRenderbuffers(1, &depth); glDeleteTextures(1, &color); glDeleteFramebuffers(1, &fbo);
        }
        Target(const Target &) = delete;
        Target &operator=(const Target &) = delete;
    };
}

struct HdrPipeline::Impl
{
    std::unique_ptr<Target> target;
    Shader output = Shader::fromSource(PbrShaders::outputVertex, PbrShaders::outputFragment, "HDR output");
    GLuint vao = 0;
    bool drawing = false;
    Impl() { glGenVertexArrays(1, &vao); if (!vao) { throw std::runtime_error("Cannot create output VAO"); } }
    ~Impl() { glDeleteVertexArrays(1, &vao); }
};

HdrPipeline::HdrPipeline() = default;
HdrPipeline::~HdrPipeline() = default;

void HdrPipeline::render(int width, int height, const glm::vec4 &clearColor,
    const std::function<void()> &draw, float exposure, bool toneMapping)
{
    if (!draw || width <= 0 || height <= 0 || !std::isfinite(exposure) || exposure <= 0 || exposure > 10000)
    { throw std::invalid_argument("Invalid HDR render size/callback/exposure"); }
    for (int i = 0; i < 4; ++i)
    {
        if (!std::isfinite(clearColor[i])) { throw std::invalid_argument("Non-finite HDR clear color"); }
    }
    if (impl_ && impl_->drawing) { throw std::logic_error("Cannot recursively render HDR frame"); }
    const State state;
    GLint textureLimit, bufferLimit;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureLimit); glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &bufferLimit);
    if (width > textureLimit || height > textureLimit || width > bufferLimit || height > bufferLimit)
    { throw std::invalid_argument("HDR dimensions exceed device limits"); }
    if (!impl_) { impl_ = std::make_unique<Impl>(); }
    if (!impl_->target || impl_->target->width != width || impl_->target->height != height)
    { impl_->target = std::make_unique<Target>(width, height); }
    impl_->drawing = true;
    struct DrawingGuard { bool &flag; ~DrawingGuard() { flag = false; } } guard{impl_->drawing};
    for (auto capability : state.capabilities) { glDisable(capability); }
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->target->fbo);
    glViewport(0, 0, width, height); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glDepthMask(GL_TRUE);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a); glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw(); // Scene::render在这里完成所有线性光照和非预乘Alpha混合。

    // 最后只输出一次；即使调用者开启FRAMEBUFFER_SRGB，也不能对已编码颜色再编码。
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.drawFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, state.readFbo);
    glViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
    for (auto capability : state.capabilities) { glDisable(capability); }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    impl_->output.use();
    impl_->output.setInt("linearImage", 0); impl_->output.setFloat("exposure", exposure);
    impl_->output.setInt("toneMapping", toneMapping);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, impl_->target->color);
    glBindVertexArray(impl_->vao); glDrawArrays(GL_TRIANGLES, 0, 3);
}
