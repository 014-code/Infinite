#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
    // 从文件中读取着色器源代码
    std::string readShaderSource(const std::string &path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::runtime_error("Failed to open shader file: " + path);
        }

        std::ostringstream source;
        source << file.rdbuf();
        return source.str();
    }

    // 获取着色器编译错误日志
    std::string getShaderInfoLog(GLuint shader)
    {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength <= 0)
        {
            return {};
        }

        std::vector<char> infoLog(static_cast<size_t>(logLength));
        glGetShaderInfoLog(shader, logLength, nullptr, infoLog.data());
        return infoLog.data();
    }

    // 获取着色器程序链接错误日志
    std::string getProgramInfoLog(GLuint program)
    {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength <= 0)
        {
            return {};
        }

        std::vector<char> infoLog(static_cast<size_t>(logLength));
        glGetProgramInfoLog(program, logLength, nullptr, infoLog.data());
        return infoLog.data();
    }

    // 编译单个着色器
    GLuint compileShader(GLenum shaderType, const std::string &source)
    {
        // 创建着色器对象
        GLuint shader = glCreateShader(shaderType);
        const char *sourcePtr = source.c_str();
        glShaderSource(shader, 1, &sourcePtr, nullptr);
        glCompileShader(shader);

        GLint success = GL_FALSE;
        // 检查编译是否成功
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            // 输出编译错误日志
            std::string infoLog = getShaderInfoLog(shader);
            std::cerr << "Shader compilation failed:\n"
                      << infoLog << std::endl;
            glDeleteShader(shader);
            throw std::runtime_error("Shader compilation failed");
        }

        // 返回编译成功的着色器对象
        return shader;
    }
}

Shader::Shader(const std::string &vertexPath, const std::string &fragmentPath)
{
    // 读取顶点着色器和片段着色器的源代码
    const std::string vertexSource = readShaderSource(vertexPath);
    const std::string fragmentSource = readShaderSource(fragmentPath);

    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;

    try
    {
        vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        // 创建着色器程序并链接
        program_ = glCreateProgram();
        glAttachShader(program_, vertexShader);
        glAttachShader(program_, fragmentShader);
        glLinkProgram(program_);

        GLint success = GL_FALSE;
        // 检查链接是否成功
        glGetProgramiv(program_, GL_LINK_STATUS, &success);
        if (!success)
        {
            // 输出链接错误日志
            std::string infoLog = getProgramInfoLog(program_);
            std::cerr << "Shader program linking failed:\n"
                      << infoLog << std::endl;
            throw std::runtime_error("Shader program linking failed");
        }
    }
    catch (...)
    {
        // 初始化过程中出错时，释放已经创建的临时资源
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program_);
        program_ = 0;
        throw;
    }

    // 删除着色器对象，因为它们已经链接到程序中
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

// Shader对象销毁时，释放它拥有的GPU着色器程序
Shader::~Shader()
{
    if (program_ != 0)
    {
        glDeleteProgram(program_);
    }
}

// 移动构造：把other拥有的GPU资源转移给当前对象
Shader::Shader(Shader &&other) noexcept
    : program_(other.program_)
{
    // 清空原对象的句柄，防止它析构时重复释放资源
    other.program_ = 0;
}

// 移动赋值：先释放当前资源，再接管other的GPU资源
Shader &Shader::operator=(Shader &&other) noexcept
{
    if (this != &other)
    {
        // 处理当前对象原来拥有的资源
        if (program_ != 0)
        {
            glDeleteProgram(program_);
        }

        // 转移资源所有权
        program_ = other.program_;

        // 让原对象变成不再拥有资源的空对象
        other.program_ = 0;
    }

    return *this;
}

// 将当前着色器程序设置为OpenGL正在使用的程序
void Shader::use() const
{
    glUseProgram(program_);
}
