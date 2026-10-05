#include "graphics/resources/Shader.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <utility>

namespace
{
    // 从文件中读取着色器源代码
    std::string readShaderSource(const std::filesystem::path &path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::runtime_error("Failed to open shader file: " + path.u8string());
        }

        std::ostringstream source;
        source << file.rdbuf();
        if (file.bad()) { throw std::runtime_error("Failed to read shader file: " + path.u8string()); }
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
    GLuint compileShader(GLenum shaderType, const std::string &source, const std::string &path)
    {
        // 创建着色器对象
        GLuint shader = glCreateShader(shaderType);
        if (shader == 0) { throw std::runtime_error("Failed to create shader: " + path); }
        const char *sourcePtr = source.c_str();
        glShaderSource(shader, 1, &sourcePtr, nullptr);
        glCompileShader(shader);

        GLint success = GL_FALSE;
        // 检查编译是否成功
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            // 保留编译错误日志和资源路径，由应用入口统一输出，避免重复记录同一异常。
            std::string infoLog = getShaderInfoLog(shader);
            glDeleteShader(shader);
            throw std::runtime_error("Shader compilation failed '" + path + "':\n" + infoLog);
        }

        // 返回编译成功的着色器对象
        return shader;
    }
}

Shader::Shader(const std::filesystem::path &vertexPath, const std::filesystem::path &fragmentPath)
    : Shader(readShaderSource(vertexPath), readShaderSource(fragmentPath), vertexPath.u8string(), fragmentPath.u8string())
{
    // 从文件读取源代码后委托给同一个编译入口，避免文件Shader与内置Shader两套清理逻辑。
}

Shader Shader::fromSource(const std::string &vertexSource, const std::string &fragmentSource, const std::string &debugName)
{
    return Shader(vertexSource, fragmentSource, debugName + " vertex", debugName + " fragment");
}

Shader::Shader(const std::string &vertexSource, const std::string &fragmentSource,
    const std::string &vertexLabel, const std::string &fragmentLabel)
{
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;

    try
    {
        vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, vertexLabel);
        fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, fragmentLabel);

        // 创建着色器程序并链接
        program_ = glCreateProgram();
        if (program_ == 0) { throw std::runtime_error("Failed to create shader program"); }
        glAttachShader(program_, vertexShader);
        glAttachShader(program_, fragmentShader);
        glLinkProgram(program_);

        GLint success = GL_FALSE;
        // 检查链接是否成功
        glGetProgramiv(program_, GL_LINK_STATUS, &success);
        if (!success)
        {
            // 链接涉及两个Shader文件，异常中同时保留路径和驱动提供的详细日志。
            std::string infoLog = getProgramInfoLog(program_);
            throw std::runtime_error("Shader program linking failed '" + vertexLabel
                + "' + '" + fragmentLabel + "':\n" + infoLog);
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
    : program_(other.program_), uniformLocations_(std::move(other.uniformLocations_)),
      matrixArrayCapacities_(std::move(other.matrixArrayCapacities_))
{
    // 清空原对象的句柄，防止它析构时重复释放资源
    other.program_ = 0;
    other.uniformLocations_.clear();
    other.matrixArrayCapacities_.clear();
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
        uniformLocations_ = std::move(other.uniformLocations_);
        matrixArrayCapacities_ = std::move(other.matrixArrayCapacities_);

        // 让原对象变成不再拥有资源的空对象
        other.program_ = 0;
        other.uniformLocations_.clear();
        other.matrixArrayCapacities_.clear();
    }

    return *this;
}

// 将当前着色器程序设置为OpenGL正在使用的程序
void Shader::use() const
{
    glUseProgram(program_);
}

bool Shader::hasUniform(const std::string &name) const
{
    return uniformLocation(name) != -1;
}

std::size_t Shader::matrixArrayCapacity(const std::string &name) const
{
    if (!program_) { return 0; }
    const auto cached = matrixArrayCapacities_.find(name);
    if (cached != matrixArrayCapacities_.end()) { return cached->second; }
    const char *pointer = name.c_str();
    GLuint index = GL_INVALID_INDEX;
    glGetUniformIndices(program_, 1, &pointer, &index);
    GLint size = 0, type = 0;
    if (index != GL_INVALID_INDEX)
    {
        glGetActiveUniformsiv(program_, 1, &index, GL_UNIFORM_SIZE, &size);
        glGetActiveUniformsiv(program_, 1, &index, GL_UNIFORM_TYPE, &type);
    }
    const auto capacity = type == GL_FLOAT_MAT4 && size > 0 ? static_cast<std::size_t>(size) : 0;
    matrixArrayCapacities_.emplace(name, capacity);
    return capacity;
}

void Shader::setMat4Array(const std::string &name, const std::vector<glm::mat4> &matrices) const
{
    if (matrices.size() > matrixArrayCapacity(name))
    { throw std::invalid_argument("Shader matrix array capacity exceeded: " + name); }
    if (!matrices.empty())
    { glUniformMatrix4fv(uniformLocation(name), static_cast<GLsizei>(matrices.size()), GL_FALSE, &matrices[0][0][0]); }
}

GLint Shader::uniformLocation(const std::string &name) const
{
    // 移走资源后的空对象没有任何uniform，不能拿program 0向驱动查询。
    if (program_ == 0) { return -1; }
    const auto found = uniformLocations_.find(name);
    if (found != uniformLocations_.end()) { return found->second; }
    const GLint location = glGetUniformLocation(program_, name.c_str());
    uniformLocations_.emplace(name, location);
    return location;
}

// 查找uniform变量位置，并上传矩阵数据
void Shader::setMat4(const std::string &name, const glm::mat4 &matrix) const
{
    const GLint location = uniformLocation(name);
    if (location != -1)
    {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }
}

void Shader::setMat3(const std::string &name, const glm::mat3 &matrix) const
{
    const GLint location = uniformLocation(name);
    if (location != -1)
    {
        glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }
}

// 查找uniform变量位置，并上传整数数据
void Shader::setInt(const std::string &name, int value) const
{
    const GLint location = uniformLocation(name);
    if (location != -1)
    {
        glUniform1i(location, value);
    }
}

// 查找uniform变量位置，并上传四维向量数据
void Shader::setVec4(const std::string &name, const glm::vec4 &value) const
{
    const GLint location = uniformLocation(name);
    if (location != -1)
    {
        glUniform4fv(location, 1, &value[0]);
    }
}

void Shader::setVec3(const std::string &name, const glm::vec3 &value) const
{
    const GLint location = uniformLocation(name);
    if (location != -1)
    {
        glUniform3fv(location, 1, glm::value_ptr(value));
    }
}

void Shader::setFloat(const std::string &name, float value) const
{
    const GLint location = uniformLocation(name);
    if (location != -1)
    {
        glUniform1f(location, value);
    }
}
