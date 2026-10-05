#pragma once

#include <GL/glew.h>
#include "../TestSupport.h"

// 仅用于测试的固定64x64离屏目标，避开窗口DPI和桌面尺寸对像素断言的影响。
// 必须在Window之后构造、之前析构；调用方在测试开始前设置自己需要的GL状态。
class ColorDepthTarget final
{
public:
    ColorDepthTarget()
    {
        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glGenRenderbuffers(1, &color_);
        glBindRenderbuffer(GL_RENDERBUFFER, color_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 64, 64);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_);
        glGenRenderbuffers(1, &depth_);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            release();
            throw std::runtime_error("Color/depth test framebuffer is incomplete");
        }
        glViewport(0, 0, 64, 64);
    }
    ~ColorDepthTarget() { release(); }
    ColorDepthTarget(const ColorDepthTarget &) = delete;
    ColorDepthTarget &operator=(const ColorDepthTarget &) = delete;

private:
    void release()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &color_);
        glDeleteRenderbuffers(1, &depth_);
        glDeleteFramebuffers(1, &framebuffer_);
    }
    GLuint framebuffer_ = 0;
    GLuint color_ = 0;
    GLuint depth_ = 0;
};
