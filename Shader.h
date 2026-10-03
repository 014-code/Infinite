#pragma once

#include <GL/glew.h>

#include <string>

class Shader
{
public:
    // 从文件读取、编译并链接顶点着色器和片段着色器
    Shader(const std::string &vertexPath, const std::string &fragmentPath);

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

private:
    GLuint program_ = 0;
};
