#include "graphics/geometry/Mesh.h"

#include <glm/common.hpp>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace
{
    std::vector<Vertex> unpackVertices(const std::vector<float> &values)
    {
        constexpr std::size_t floatsPerVertex = 8;
        if (values.empty() || values.size() % floatsPerVertex != 0)
        {
            throw std::invalid_argument("Mesh vertices require position XYZ, color RGB and UV");
        }
        std::vector<Vertex> result;
        result.reserve(values.size() / floatsPerVertex);
        for (std::size_t offset = 0; offset < values.size(); offset += floatsPerVertex)
        {
            result.push_back({{values[offset], values[offset + 1], values[offset + 2]},
                {values[offset + 3], values[offset + 4], values[offset + 5]},
                {values[offset + 6], values[offset + 7]}});
        }
        return result;
    }

    // 创建网格不应覆盖外部VAO/VBO；EBO绑定属于VAO，恢复VAO即可恢复原EBO。
    struct MeshUploadState
    {
        GLint vao = 0;
        GLint buffer = 0;
        MeshUploadState()
        {
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        }
        ~MeshUploadState()
        {
            glBindVertexArray(static_cast<GLuint>(vao));
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
        }
    };

    void upload(GLenum target, GLsizeiptr bytes, const void *data)
    {
        glBufferData(target, bytes, data, GL_STATIC_DRAW);
        GLint64 allocated = 0;
        glGetBufferParameteri64v(target, GL_BUFFER_SIZE, &allocated);
        if (allocated != bytes) { throw std::runtime_error("Failed to allocate Mesh buffer"); }
    }
}

// 旧的8-float布局只在此转换一次，其余上传和绘制逻辑共用新实现。
Mesh::Mesh(const std::vector<float> &vertices) : Mesh(unpackVertices(vertices))
{
}

Mesh::Mesh(const MeshData &data, const std::vector<SkinVertex> &skin) : Mesh(data.vertices, data.indices, skin)
{
}

Mesh::Mesh(const std::vector<Vertex> &vertices, const std::vector<std::uint32_t> &indices,
    const std::vector<SkinVertex> &skin)
{
    static_assert(std::is_standard_layout<Vertex>::value, "Vertex must support offsetof");
    const std::size_t countLimit = static_cast<std::size_t>(std::numeric_limits<GLsizei>::max());
    const std::size_t bytesLimit = static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max());
    if (vertices.empty() || vertices.size() > countLimit || indices.size() > countLimit ||
        vertices.size() > bytesLimit / sizeof(Vertex) || indices.size() > bytesLimit / sizeof(std::uint32_t))
    {
        throw std::invalid_argument("Mesh is empty or exceeds OpenGL buffer limits");
    }
    if ((indices.empty() ? vertices.size() : indices.size()) % 3 != 0)
    {
        throw std::invalid_argument("Mesh requires complete triangles");
    }
    for (auto index : indices)
    {
        if (index >= vertices.size()) { throw std::invalid_argument("Mesh index is out of range"); }
    }
    bounds_.minimum = bounds_.maximum = vertices.front().position;
    for (const auto &vertex : vertices)
    {
        for (float value : {vertex.position.x, vertex.position.y, vertex.position.z,
            vertex.color.r, vertex.color.g, vertex.color.b, vertex.colorAlpha, vertex.uv.x, vertex.uv.y,
            vertex.tangent.x, vertex.tangent.y, vertex.tangent.z, vertex.tangent.w})
        {
            if (!std::isfinite(value)) { throw std::invalid_argument("Mesh attributes must be finite"); }
        }
        for (float value : {vertex.normal.x, vertex.normal.y, vertex.normal.z})
        {
            if (!std::isfinite(value)) { throw std::invalid_argument("Mesh normals must be finite"); }
        }
        bounds_.minimum = glm::min(bounds_.minimum, vertex.position);
        bounds_.maximum = glm::max(bounds_.maximum, vertex.position);
    }
    vertexCount_ = static_cast<GLsizei>(vertices.size());
    indexCount_ = static_cast<GLsizei>(indices.size());
    if (!skin.empty() && (skin.size() != vertices.size() || skin.size() > bytesLimit / sizeof(SkinVertex)))
    { throw std::invalid_argument("Skin attributes must match mesh vertex count"); }
    for (const auto &vertex : skin)
    {
        float sum = 0;
        for (int component = 0; component < 4; ++component)
        {
            const float weight = vertex.weights[component];
            if (!std::isfinite(weight) || weight < 0 || weight > 1)
            { throw std::invalid_argument("Invalid skin vertex weight"); }
            sum += weight;
            maximumJoint_ = std::max(maximumJoint_, vertex.joints[component]);
        }
        if (std::abs(sum - 1) > .001f) { throw std::invalid_argument("Mesh skin weights must be normalized"); }
    }
    const MeshUploadState savedState;
    try
    {
        // 创建顶点数组对象和顶点缓冲对象；索引网格再创建EBO。
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        if (vao_ == 0 || vbo_ == 0) { throw std::runtime_error("Failed to create Mesh"); }

        // 绑定VAO和VBO，将顶点数据复制到GPU的顶点缓冲区。
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        upload(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data());

        // 告诉OpenGL如何解析每个顶点的位置、颜色、纹理坐标和法线数据。
        // 当前示例Shader可能暂时不读取法线，但提前上传可以让OBJ数据保留给后续光照模块。
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, color)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, uv)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, tangent)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, colorAlpha)));
        glEnableVertexAttribArray(5);
        if (!skin.empty())
        {
            static_assert(std::is_standard_layout<SkinVertex>::value, "SkinVertex must support offsetof");
            glGenBuffers(1, &skinVbo_);
            if (!skinVbo_) { throw std::runtime_error("Failed to create skin attribute buffer"); }
            glBindBuffer(GL_ARRAY_BUFFER, skinVbo_);
            upload(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(skin.size() * sizeof(SkinVertex)), skin.data());
            // 索引必须用整数属性入口，不能让glVertexAttribPointer把整数转换成浮点数。
            glVertexAttribIPointer(6, 4, GL_UNSIGNED_INT, sizeof(SkinVertex), reinterpret_cast<void *>(offsetof(SkinVertex, joints)));
            glEnableVertexAttribArray(6);
            glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(SkinVertex), reinterpret_cast<void *>(offsetof(SkinVertex, weights)));
            glEnableVertexAttribArray(7);
        }
        if (!indices.empty())
        {
            glGenBuffers(1, &ebo_);
            if (ebo_ == 0) { throw std::runtime_error("Failed to create Mesh index buffer"); }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
            // EBO绑定属于当前VAO，会随VAO一起保存；draw时只需恢复VAO即可找到对应索引缓冲。
            upload(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)), indices.data());
        }
        // 解绑并恢复原绑定，避免后续操作意外修改当前顶点对象；由savedState自动执行。
    }
    catch (...)
    {
        // 构造失败时对象析构不会执行，必须释放已经分配的部分资源。
        release();
        throw;
    }
}

// Mesh对象销毁时，释放它拥有的VAO和VBO，也包括可选EBO。
Mesh::~Mesh() { release(); }

void Mesh::release() noexcept
{
    if (vao_ != 0) { glDeleteVertexArrays(1, &vao_); }
    if (vbo_ != 0) { glDeleteBuffers(1, &vbo_); }
    if (ebo_ != 0) { glDeleteBuffers(1, &ebo_); }
    if (skinVbo_ != 0) { glDeleteBuffers(1, &skinVbo_); }
    skinVbo_ = 0; maximumJoint_ = 0;
    vao_ = vbo_ = ebo_ = 0;
    vertexCount_ = indexCount_ = 0;
    bounds_ = {};
}

// 移动构造：把other的VAO、VBO、EBO和顶点数量转移给当前对象。
Mesh::Mesh(Mesh &&other) noexcept
    : vao_(std::exchange(other.vao_, 0)), vbo_(std::exchange(other.vbo_, 0)),
      ebo_(std::exchange(other.ebo_, 0)), vertexCount_(std::exchange(other.vertexCount_, 0)),
      indexCount_(std::exchange(other.indexCount_, 0)), bounds_(std::exchange(other.bounds_, {}))
{
    skinVbo_ = std::exchange(other.skinVbo_, 0);
    maximumJoint_ = std::exchange(other.maximumJoint_, 0);
    // exchange取出旧值并清空原对象的句柄，防止重复释放OpenGL资源。
}

// 移动赋值：释放当前资源，再接管other的网格资源。
Mesh &Mesh::operator=(Mesh &&other) noexcept
{
    if (this != &other)
    {
        // 释放当前对象原来拥有的资源，随后转移资源所有权。
        release();
        vao_ = std::exchange(other.vao_, 0);
        vbo_ = std::exchange(other.vbo_, 0);
        ebo_ = std::exchange(other.ebo_, 0);
        skinVbo_ = std::exchange(other.skinVbo_, 0);
        maximumJoint_ = std::exchange(other.maximumJoint_, 0);
        vertexCount_ = std::exchange(other.vertexCount_, 0);
        indexCount_ = std::exchange(other.indexCount_, 0);
        bounds_ = std::exchange(other.bounds_, {});
        // 原对象变成空对象，避免析构时重复释放。
    }
    return *this;
}

GLsizei Mesh::vertexCount() const { return vertexCount_; }
GLsizei Mesh::indexCount() const { return indexCount_; }
const MeshBounds &Mesh::bounds() const { return bounds_; }

// 绑定当前网格的VAO，并按照顶点或索引数量发起绘制，然后恢复调用者VAO。
void Mesh::draw() const
{
    if (vao_ == 0) { throw std::logic_error("Cannot draw a moved-from Mesh"); }
    GLint previous = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous);
    glBindVertexArray(vao_);
    if (indexCount_ != 0) { glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr); }
    else { glDrawArrays(GL_TRIANGLES, 0, vertexCount_); }
    glBindVertexArray(static_cast<GLuint>(previous));
}
