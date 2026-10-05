#pragma once

#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <filesystem>
#include <unordered_map>
#include <vector>

class Shader
{
public:
    // 从文件读取、编译并链接顶点着色器和片段着色器
    Shader(const std::filesystem::path &vertexPath, const std::filesystem::path &fragmentPath);
    // 内置资源可直接从内存编译，不必写临时文件。debugName仅用于编译/链接错误诊断。
    static Shader fromSource(const std::string &vertexSource, const std::string &fragmentSource,
        const std::string &debugName = "memory shader");

    // 对象销毁时释放GPU中的着色器程序
    ~Shader();

    // Shader拥有GPU资源，禁止复制，避免两个对象重复释放同一个资源
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;

    // 允许转移GPU资源的所有权，不复制实际的OpenGL资源
    Shader(Shader &&other) noexcept;
    Shader &operator=(Shader &&other) noexcept;

    // 使用着色器程序
    void use() const;

    // 查询链接后仍在使用的uniform，不修改当前程序；用于按需准备法线矩阵。
    // 首次按名称查询驱动，之后复用缓存（不存在的-1也缓存）；仅限有效上下文的主线程。
    bool hasUniform(const std::string &name) const;

    // 将4x4矩阵传给着色器中的同名uniform变量。调用set函数前必须use此Shader。
    // 不存在/被优化掉的uniform保持跳过，便于纯颜色和纹理Shader共用Material。
    void setMat4(const std::string &name, const glm::mat4 &matrix) const;
    // 查询实际链接出的mat4数组容量，名称使用首元素如jointMatrices[0]；查询结果随程序缓存。
    std::size_t matrixArrayCapacity(const std::string &name) const;
    void setMat4Array(const std::string &name, const std::vector<glm::mat4> &matrices) const;

    // 上传法线矩阵。光照Shader用它把局部法线正确变换到世界空间。
    void setMat3(const std::string &name, const glm::mat3 &matrix) const;

    // 将整数传给着色器中的同名uniform变量
    void setInt(const std::string &name, int value) const;

    // 将四维向量传给着色器中的同名uniform变量
    void setVec4(const std::string &name, const glm::vec4 &value) const;

    // 上传方向光方向、颜色等三维参数。
    void setVec3(const std::string &name, const glm::vec3 &value) const;

    // 上传光照强度等单精度参数。
    void setFloat(const std::string &name, float value) const;

private:
    GLint uniformLocation(const std::string &name) const;
    Shader(const std::string &vertexSource, const std::string &fragmentSource,
        const std::string &vertexLabel, const std::string &fragmentLabel);
    GLuint program_ = 0;
    // location只对所属的已链接program有效，因此缓存随程序移动，不可跨Shader共享。
    // mutable只允许const查询填充缓存，不意味着线程安全；若以后加入重新链接，必须清空它。
    mutable std::unordered_map<std::string, GLint> uniformLocations_;
    mutable std::unordered_map<std::string, std::size_t> matrixArrayCapacities_;
};
