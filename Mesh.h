#pragma once

#include <GL/glew.h>

#include <vector>

class Mesh
{
public:
    // 创建包含位置和颜色属性的网格
    explicit Mesh(const std::vector<float> &vertices);

    // 对象销毁时释放VAO和VBO
    ~Mesh();

    // Mesh拥有OpenGL资源，禁止复制，避免重复释放同一组资源
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;

    // 允许转移VAO和VBO的所有权
    Mesh(Mesh &&other) noexcept;
    Mesh &operator=(Mesh &&other) noexcept;

    // 绘制网格
    void draw() const;

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLsizei vertexCount_ = 0;
};

// 创建一个带有红、绿、蓝三个顶点颜色的三角形
Mesh createColorTriangle();
