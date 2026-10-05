#include "MeshLoader.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct FaceIndex
    {
        // OBJ文件的索引从1开始；这里position使用0-based下标，
        // texCoord和normal保留0表示“该面没有提供这一项”。
        int position = 0;
        int texCoord = 0;
        int normal = 0;
    };

    struct VertexKey
    {
        // OBJ的一个顶点实际上是位置、UV、法线三种索引的组合。
        // 只要其中一项不同，就必须生成独立的OpenGL Vertex，不能只按位置去重。
        int position = 0;
        int texCoord = 0;
        int normal = 0;

        bool operator==(const VertexKey &other) const noexcept
        {
            return position == other.position && texCoord == other.texCoord && normal == other.normal;
        }
    };

    struct VertexKeyHash
    {
        std::size_t operator()(const VertexKey &key) const noexcept
        {
            std::size_t result = std::hash<int>{}(key.position);
            result ^= std::hash<int>{}(key.texCoord) + static_cast<std::size_t>(0x9e3779b9u) +
                (result << 6) + (result >> 2);
            result ^= std::hash<int>{}(key.normal) + static_cast<std::size_t>(0x9e3779b9u) +
                (result << 6) + (result >> 2);
            return result;
        }
    };

    int resolveIndex(int index, std::size_t count, const char *kind, std::size_t lineNumber)
    {
        // 正索引从1开始，负索引从当前属性列表末尾向前数；OBJ不允许0索引。
        // 转换后统一为C++使用的0-based下标，后续访问数组前在这里完成范围检查。
        if (index == 0)
        {
            throw std::runtime_error("Invalid OBJ " + std::string(kind) +
                " index at line " + std::to_string(lineNumber));
        }
        const long long resolved = index > 0 ? static_cast<long long>(index) - 1 :
            static_cast<long long>(count) + index;
        if (resolved < 0 || resolved >= static_cast<long long>(count))
        {
            throw std::runtime_error("OBJ " + std::string(kind) +
                " index out of range at line " + std::to_string(lineNumber));
        }
        return static_cast<int>(resolved);
    }

    FaceIndex parseFaceIndex(
        const std::string &token,
        std::size_t positionCount,
        std::size_t texCoordCount,
        std::size_t normalCount,
        std::size_t lineNumber)
    {
        FaceIndex result;
        // 一个面索引可能是v、v/vt、v//vn或v/vt/vn。
        // 空的UV/法线部分不报错，表示该面没有提供对应属性。
        std::stringstream stream(token);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(stream, part, '/')) { parts.push_back(part); }
        if (parts.empty() || parts.size() > 3 || parts[0].empty())
        {
            throw std::runtime_error("Malformed OBJ face index at line " + std::to_string(lineNumber));
        }

        try
        {
            result.position = resolveIndex(std::stoi(parts[0]), positionCount, "position", lineNumber);
            if (parts.size() >= 2 && !parts[1].empty())
            {
                result.texCoord = resolveIndex(std::stoi(parts[1]), texCoordCount, "UV", lineNumber) + 1;
            }
            if (parts.size() == 3 && !parts[2].empty())
            {
                result.normal = resolveIndex(std::stoi(parts[2]), normalCount, "normal", lineNumber) + 1;
            }
        }
        catch (const std::invalid_argument &)
        {
            throw std::runtime_error("Malformed OBJ face index at line " + std::to_string(lineNumber));
        }
        catch (const std::out_of_range &)
        {
            throw std::runtime_error("OBJ face index is too large at line " + std::to_string(lineNumber));
        }
        return result;
    }

    glm::vec3 faceNormal(
        const std::vector<glm::vec3> &positions,
        const FaceIndex &a,
        const FaceIndex &b,
        const FaceIndex &c,
        std::size_t lineNumber)
    {
        const glm::vec3 normal = glm::cross(
            positions[b.position] - positions[a.position],
            positions[c.position] - positions[a.position]);
        const float squaredLength = glm::dot(normal, normal);
        if (!std::isfinite(squaredLength) || squaredLength <= std::numeric_limits<float>::epsilon())
        {
            throw std::runtime_error("OBJ face is degenerate at line " + std::to_string(lineNumber));
        }
        return glm::normalize(normal);
    }
}

MeshData MeshLoader::loadObj(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open OBJ mesh: " + path.string());
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    MeshData result;
    std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> vertexMap;
    std::string line;
    std::size_t lineNumber = 0;
    std::size_t faceNumber = 0;

    const auto addVertex = [&](const FaceIndex &index, const glm::vec3 &generatedNormal)
    {
        // 缺失法线时按面生成法线。把faceNumber放入去重键，
        // 可以避免相邻面错误共享同一个法线，从而保留平面硬边。
        const VertexKey key{index.position, index.texCoord, index.normal != 0 ? index.normal :
            -static_cast<int>(faceNumber + 1)};
        const auto found = vertexMap.find(key);
        if (found != vertexMap.end()) { return found->second; }

        Vertex vertex;
        vertex.position = positions[index.position];
        vertex.color = glm::vec3(1.0f);
        // texCoord/normal保存的是“数组下标+1”，0专门表示缺失；访问数组时再减1。
        vertex.uv = index.texCoord == 0 ? glm::vec2(0.0f) : texCoords[index.texCoord - 1];
        vertex.normal = index.normal == 0 ? generatedNormal : normals[index.normal - 1];
        const std::uint32_t newIndex = static_cast<std::uint32_t>(result.vertices.size());
        result.vertices.push_back(vertex);
        vertexMap.emplace(key, newIndex);
        return newIndex;
    };

    while (std::getline(file, line))
    {
        ++lineNumber;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) { line.resize(comment); }
        std::istringstream stream(line);
        std::string command;
        if (!(stream >> command)) { continue; }

        if (command == "v")
        {
            glm::vec3 position;
            if (!(stream >> position.x >> position.y >> position.z) ||
                !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
            {
                throw std::runtime_error("Malformed OBJ position at line " + std::to_string(lineNumber));
            }
            positions.push_back(position);
        }
        else if (command == "vt")
        {
            glm::vec2 uv;
            if (!(stream >> uv.x >> uv.y) || !std::isfinite(uv.x) || !std::isfinite(uv.y))
            {
                throw std::runtime_error("Malformed OBJ UV at line " + std::to_string(lineNumber));
            }
            texCoords.push_back(uv);
        }
        else if (command == "vn")
        {
            glm::vec3 normal;
            if (!(stream >> normal.x >> normal.y >> normal.z) ||
                !std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z) ||
                glm::dot(normal, normal) <= std::numeric_limits<float>::epsilon())
            {
                throw std::runtime_error("Malformed OBJ normal at line " + std::to_string(lineNumber));
            }
            // 即使文件中的法线长度不是1，也统一归一化，便于后续光照计算直接使用。
            normals.push_back(glm::normalize(normal));
        }
        else if (command == "f")
        {
            std::vector<FaceIndex> face;
            std::string token;
            while (stream >> token)
            {
                face.push_back(parseFaceIndex(token, positions.size(), texCoords.size(), normals.size(), lineNumber));
            }
            if (face.size() < 3)
            {
                throw std::runtime_error("OBJ face requires at least three vertices at line " + std::to_string(lineNumber));
            }
            ++faceNumber;
            // 多边形使用扇形三角化：顶点0分别和顶点1/2、2/3组成三角形。
            // 适合本阶段示例中的凸面；复杂凹多边形应在建模工具中预先三角化。
            for (std::size_t triangle = 1; triangle + 1 < face.size(); ++triangle)
            {
                const glm::vec3 generatedNormal = faceNormal(
                    positions, face[0], face[triangle], face[triangle + 1], lineNumber);
                result.indices.push_back(addVertex(face[0], generatedNormal));
                result.indices.push_back(addVertex(face[triangle], generatedNormal));
                result.indices.push_back(addVertex(face[triangle + 1], generatedNormal));
            }
        }
        // mtllib/usemtl/o/g/s等命令留给后续Model/Material加载阶段，不影响几何解析。
    }

    if (result.vertices.empty() || result.indices.empty())
    {
        throw std::runtime_error("OBJ file does not contain any drawable faces: " + path.string());
    }
    return result;
}
