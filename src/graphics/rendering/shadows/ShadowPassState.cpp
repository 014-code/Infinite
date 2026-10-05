#include "ShadowPassState.h"

ShadowPassState::ShadowPassState()
{
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_);
    glGetIntegerv(GL_CURRENT_PROGRAM,&program_); glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao_);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active_); glGetIntegerv(GL_VIEWPORT,viewport_.data());
    glGetIntegerv(GL_DEPTH_FUNC,&depthFunction_); glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite_);
    glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clearDepth_); glGetIntegerv(GL_CULL_FACE_MODE,&cullMode_);
    glGetIntegerv(GL_FRONT_FACE,&frontFace_);
    glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture_); glActiveTexture(active_);
    for (std::size_t i=0;i<enabled_.size();++i) { enabled_[i]=glIsEnabled(capabilities_[i]); }
}

ShadowPassState::~ShadowPassState()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw_); glBindFramebuffer(GL_READ_FRAMEBUFFER,read_);
    glViewport(viewport_[0],viewport_[1],viewport_[2],viewport_[3]);
    glUseProgram(program_); glBindVertexArray(vao_);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture_); glActiveTexture(active_);
    glDepthFunc(depthFunction_); glDepthMask(depthWrite_); glClearDepth(clearDepth_);
    glCullFace(cullMode_); glFrontFace(frontFace_);
    for (std::size_t i=0;i<enabled_.size();++i)
    { if (enabled_[i]) { glEnable(capabilities_[i]); } else { glDisable(capabilities_[i]); } }
}
