#pragma once

#include <GL/glew.h>
#include "graphics/geometry/Vertex.h"
#include "assets/MeshData.h"
#include "animation/Skin.h"

#include <cstdint>
#include <vector>

class Mesh
{
public:
    // 创建包含位置、颜色和纹理坐标属性的网格
    explicit Mesh(const std::vector<float> &vertices);

    // 推荐入口。索引每三个组成一个三角形；空索引表示按顶点顺序绘制。
    // 上传后不借用CPU数据；创建/销毁/移动赋值需要当前有效的OpenGL上下文。
    explicit Mesh(const std::vector<Vertex> &vertices, const std::vector<std::uint32_t> &indices = {},
        const std::vector<SkinVertex> &skin = {});
    // 从CPU网格数据创建GPU网格；Mesh不会保留MeshData引用。
    explicit Mesh(const MeshData &data, const std::vector<SkinVertex> &skin = {});
    bool hasSkinAttributes() const noexcept { return skinVbo_ != 0; }
    std::uint32_t maximumJoint() const noexcept { return maximumJoint_; }
    GLsizei vertexCount() const;
    GLsizei indexCount() const;
    const MeshBounds &bounds() const;

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
    void release() noexcept;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLuint skinVbo_ = 0; // 仅蒙皮网格分配，静态顶点没有这份开销。
    std::uint32_t maximumJoint_ = 0;
    GLsizei vertexCount_ = 0;
    GLsizei indexCount_ = 0;
    MeshBounds bounds_;
};
