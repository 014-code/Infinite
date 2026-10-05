#pragma once
#include <GL/glew.h>
#include <array>

// 深度通道触及的状态集中在一处。FBO的draw/read buffer选择属于各自FBO，
// 不修改调用者FBO的附件或选择；异常退出也恢复原FBO、viewport和纹理绑定。
class ShadowPassState final
{
public:
    ShadowPassState();
    ~ShadowPassState();
    ShadowPassState(const ShadowPassState &) = delete;
    ShadowPassState &operator=(const ShadowPassState &) = delete;
private:
    GLint draw_,read_,program_,vao_,active_,texture_,depthFunction_,cullMode_,frontFace_;
    std::array<GLint,4> viewport_;
    GLboolean depthWrite_;
    GLdouble clearDepth_;
    const std::array<GLenum,6> capabilities_{GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_POLYGON_OFFSET_FILL};
    std::array<GLboolean,6> enabled_;
};
