#include "graphics/rendering/OpenGLDebug.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "core/Log.h"

#include <sstream>

namespace
{
    const char *errorName(GLenum error)
    {
        switch (error)
        {
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        default: return "GL_UNKNOWN_ERROR";
        }
    }
}

bool OpenGLDebug::checkErrors(std::string_view operation, const char *file, int line)
{
    // GLFW管理本项目的上下文；不依赖OpenGL 4.3或KHR_debug调试回调扩展。
    if (glfwGetCurrentContext() == nullptr)
    {
        Log::write(Log::Level::Error,
            std::string(operation) + ": no current OpenGL context", file, line);
        return false;
    }

    bool success = true;
    for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
    {
        success = false;
        std::ostringstream message;
        message << operation << ": " << errorName(error) << " (0x"
                << std::hex << std::uppercase << error << ')';
        // 即使日志被过滤，也必须返回false并读完错误状态，不能把静默等同于成功。
        Log::write(Log::Level::Error, message.str(), file, line);
    }
    return success;
}
