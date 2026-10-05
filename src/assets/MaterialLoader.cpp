#include "MaterialLoader.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <locale>

namespace
{
    constexpr int materialFormatVersion = 1;

    std::filesystem::path absoluteNormalized(const std::filesystem::path &path)
    {
        // 解析阶段先固定材质文件自身的位置；材质内部的相对路径都以此文件所在目录为基准。
        if (path.empty()) { throw std::invalid_argument("Material path must not be empty"); }
        return std::filesystem::absolute(path).lexically_normal();
    }

    std::filesystem::path resolveAssetPath(
        const std::filesystem::path &materialPath,
        const std::filesystem::path &assetPath)
    {
        // 这里不要求资源此刻存在，把“路径解析”和“GPU资源加载”分开，
        // 由ResourceManager在真正加载Shader/Texture时给出更具体的错误。
        if (assetPath.empty()) { throw std::runtime_error("Material asset path must not be empty"); }
        if (assetPath.is_absolute()) { return assetPath.lexically_normal(); }
        return (materialPath.parent_path() / assetPath).lexically_normal();
    }

    void requireToken(std::istream &stream, const char *expected, const char *context)
    {
        std::string actual;
        if (!(stream >> actual) || actual != expected)
        {
            throw std::runtime_error(std::string("Invalid material file: expected ") + expected +
                " while reading " + context);
        }
    }

    void requireFinite(const glm::vec4 &color)
    {
        for (int index = 0; index < 4; ++index)
        {
            if (!std::isfinite(color[index]))
            {
                throw std::runtime_error("Invalid material file: base color must be finite");
            }
        }
        if (color.a < 0.0f || color.a > 1.0f)
        {
            throw std::runtime_error("Invalid material file: base color alpha must be in [0, 1]");
        }
    }
}

MaterialData MaterialLoader::load(const std::filesystem::path &path)
{
    const std::filesystem::path materialPath = absoluteNormalized(path);
    std::ifstream file(materialPath);
    file.imbue(std::locale::classic());
    if (!file)
    {
        throw std::runtime_error("Failed to open material file: " + materialPath.string());
    }

    requireToken(file, "INFINITE_MATERIAL", "file header");
    int version = 0;
    if (!(file >> version) || version != materialFormatVersion)
    {
        throw std::runtime_error("Unsupported material file version");
    }

    MaterialData result;
    bool hasVertexShader = false;
    bool hasFragmentShader = false;
    bool hasBaseColor = false;
    bool hasRenderMode = false;
    bool hasCullMode = false;
    bool ended = false;
    std::unordered_set<std::string> fields;
    std::string field;
    while (file >> field)
    {
        // 重复配置通常来自复制粘贴错误，不能让后一个值悄悄覆盖前一个。
        if (!fields.insert(field).second)
        {
            throw std::runtime_error("Duplicate material field: " + field);
        }
        if (field == "VERTEX_SHADER")
        {
            std::string value;
            if (!(file >> std::quoted(value)))
            {
                throw std::runtime_error("Invalid material file: malformed vertex shader path");
            }
            result.vertexShaderPath = resolveAssetPath(materialPath, std::filesystem::u8path(value));
            hasVertexShader = true;
        }
        else if (field == "FRAGMENT_SHADER")
        {
            std::string value;
            if (!(file >> std::quoted(value)))
            {
                throw std::runtime_error("Invalid material file: malformed fragment shader path");
            }
            result.fragmentShaderPath = resolveAssetPath(materialPath, std::filesystem::u8path(value));
            hasFragmentShader = true;
        }
        else if (field == "TEXTURE")
        {
            std::string value;
            if (!(file >> std::quoted(value)))
            {
                throw std::runtime_error("Invalid material file: malformed texture path");
            }
            if (value == "NONE")
            {
                // NONE是材质格式中的显式无纹理标记，而不是名为NONE的文件。
                result.hasTexture = false;
                result.texturePath.clear();
            }
            else
            {
                result.hasTexture = true;
                result.texturePath = resolveAssetPath(materialPath, std::filesystem::u8path(value));
            }
        }
        else if (field == "BASE_COLOR")
        {
            if (!(file >> result.baseColor.r >> result.baseColor.g >>
                result.baseColor.b >> result.baseColor.a))
            {
                throw std::runtime_error("Invalid material file: malformed base color");
            }
            requireFinite(result.baseColor);
            hasBaseColor = true;
        }
        else if (field == "RENDER_MODE")
        {
            std::string value;
            if (!(file >> value)) { throw std::runtime_error("Invalid material file: missing render mode"); }
            if (value == "Opaque") { result.renderMode = RenderMode::Opaque; }
            else if (value == "AlphaBlend") { result.renderMode = RenderMode::AlphaBlend; }
            else { throw std::runtime_error("Invalid material file: unknown render mode"); }
            hasRenderMode = true;
        }
        else if (field == "CULL_MODE")
        {
            std::string value;
            if (!(file >> value)) { throw std::runtime_error("Invalid material file: missing cull mode"); }
            if (value == "None") { result.cullMode = CullMode::None; }
            else if (value == "Back") { result.cullMode = CullMode::Back; }
            else { throw std::runtime_error("Invalid material file: unknown cull mode"); }
            hasCullMode = true;
        }
        else if (field == "END_MATERIAL")
        {
            ended = true;
            break;
        }
        else
        {
            throw std::runtime_error("Invalid material file: unknown field " + field);
        }
    }

    if (!ended || !fields.count("TEXTURE") || !hasVertexShader ||
        !hasFragmentShader || !hasBaseColor || !hasRenderMode || !hasCullMode)
    {
        // 所有字段都必须出现，避免材质缺一项时悄悄使用运行时默认值。
        throw std::runtime_error("Invalid material file: required field is missing");
    }
    std::string unexpected;
    if (file >> unexpected)
    {
        throw std::runtime_error("Invalid material file: unexpected data after END_MATERIAL");
    }
    return result;
}
