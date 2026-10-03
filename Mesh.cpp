#include "Mesh.h"

#include <stdexcept>

Mesh::Mesh(const std::vector<float> &vertices)
{
    constexpr size_t floatsPerVertex = 6;
    if (vertices.empty() || vertices.size() % floatsPerVertex != 0)
    {
        throw std::invalid_argument(
            "Mesh vertices must contain position and color data for every vertex");
    }

    vertexCount_ = static_cast<GLsizei>(vertices.size() / floatsPerVertex);

    // 创建顶点数组对象和顶点缓冲对象
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    // 绑定VAO和VBO
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // 将顶点数据复制到GPU的顶点缓冲区
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float),
        vertices.data(),
        GL_STATIC_DRAW);

    // 告诉OpenGL如何解析每个顶点的位置数据
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(floatsPerVertex * sizeof(float)),
        nullptr);
    glEnableVertexAttribArray(0);

    // 告诉OpenGL如何解析每个顶点的颜色数据
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(floatsPerVertex * sizeof(float)),
        reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑，避免后续操作意外修改当前顶点对象
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Mesh对象销毁时，释放它拥有的VAO和VBO
Mesh::~Mesh()
{
    if (vao_ != 0)
    {
        glDeleteVertexArrays(1, &vao_);
    }
    if (vbo_ != 0)
    {
        glDeleteBuffers(1, &vbo_);
    }
}

// 移动构造：把other的VAO、VBO和顶点数量转移给当前对象
Mesh::Mesh(Mesh &&other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), vertexCount_(other.vertexCount_)
{
    // 清空原对象的句柄，防止重复释放OpenGL资源
    other.vao_ = 0;
    other.vbo_ = 0;
    other.vertexCount_ = 0;
}

// 移动赋值：释放当前资源，再接管other的网格资源
Mesh &Mesh::operator=(Mesh &&other) noexcept
{
    if (this != &other)
    {
        // 释放当前对象原来拥有的资源
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);

        // 转移资源所有权
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        vertexCount_ = other.vertexCount_;

        // 让原对象变成空对象，避免析构时重复释放
        other.vao_ = 0;
        other.vbo_ = 0;
        other.vertexCount_ = 0;
    }

    return *this;
}

// 绑定当前网格的VAO，并按照顶点数量发起绘制
void Mesh::draw() const
{
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
    glBindVertexArray(0);
}

Mesh createColorTriangle()
{
    // 定义三个顶点，每个顶点包含位置和颜色
    const std::vector<float> vertices = {
        -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, // 左下角，红色
         0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, // 右下角，绿色
         0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f  // 顶部，蓝色
    };

    return Mesh(vertices);
}
