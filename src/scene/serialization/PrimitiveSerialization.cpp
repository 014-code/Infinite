#include "PrimitiveSerialization.h"

#include "graphics/geometry/primitives/PrimitiveMeshBuilder.h"
#include "graphics/resources/Material.h"
#include "scene/components/Renderable.h"
#include <array>
#include <charconv>
#include <cmath>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace scene_serialization
{
    namespace
    {
        constexpr std::array<const char *, 6> names{{"Cube", "Plane", "Disk", "Sphere", "Cylinder", "Cone"}};

        void token(std::istream &stream, const char *expected)
        {
            std::string actual;
            if (!(stream >> actual) || actual != expected)
            {
                throw std::runtime_error(std::string("Invalid scene primitive: expected ") + expected);
            }
        }

        void checkColor(const glm::vec4 &color)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (!std::isfinite(color[i])) { throw std::runtime_error("Invalid scene primitive color"); }
            }
            if (color.a < 0 || color.a > 1) { throw std::runtime_error("Invalid scene primitive alpha"); }
        }

        std::uint32_t readCount(std::istream &stream)
        {
            std::string text;
            std::uint32_t result = 0;
            if (!(stream >> text)) { throw std::runtime_error("Missing primitive segment count"); }
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
            // 不能直接把带负号的文本读入unsigned，某些值会回绕成合法的小正数。
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            {
                throw std::runtime_error("Invalid primitive segment count");
            }
            return result;
        }
    }

    std::optional<PrimitiveState> readPrimitive(std::istream &stream)
    {
        token(stream, "PRIMITIVE");
        std::string name;
        if (!(stream >> name)) { throw std::runtime_error("Missing scene primitive type"); }
        if (name == "NONE") { return std::nullopt; }
        std::size_t type = 0;
        while (type < names.size() && name != names[type]) { ++type; }
        if (type == names.size()) { throw std::runtime_error("Unknown scene primitive: " + name); }
        PrimitiveState result;
        auto &d = result.geometry;
        d.type = static_cast<PrimitiveType>(type);
        token(stream, "SIZE"); stream >> d.size.x >> d.size.y >> d.size.z;
        token(stream, "RADIUS"); stream >> d.radius;
        token(stream, "HEIGHT"); stream >> d.height;
        token(stream, "RADIAL_SEGMENTS"); d.radialSegments = readCount(stream);
        token(stream, "LATITUDE_SEGMENTS"); d.latitudeSegments = readCount(stream);
        token(stream, "COLOR"); stream >> result.color.r >> result.color.g >> result.color.b >> result.color.a;
        if (!stream) { throw std::runtime_error("Malformed scene primitive data"); }
        checkColor(result.color);
        // 解析阶段就验证大小和分段上限，不等创建窗口/GPU网格后才发现数据非法。
        try { d = PrimitiveMeshBuilder::canonicalize(d); }
        catch (const std::invalid_argument &error) { throw std::runtime_error(error.what()); }
        return result;
    }

    void writePrimitive(std::ostream &stream, const Renderable &renderable)
    {
        if (!renderable.primitiveDescription()) { stream << "PRIMITIVE NONE\n"; return; }
        const bool fileMaterial = !renderable.materialPath().empty();
        if ((!renderable.hasBuiltinPrimitiveMaterial() && !fileMaterial) || !renderable.material())
        {
            throw std::invalid_argument("Cannot save a primitive with a runtime custom Material; use builtin material or materialPath");
        }
        const auto d = PrimitiveMeshBuilder::canonicalize(*renderable.primitiveDescription());
        const auto &material = *renderable.material();
        // 文件材质由MATERIAL字段重建，COLOR占位为白色；不把缓存材质运行时改动伪装成已写回文件。
        const auto color = fileMaterial ? glm::vec4(1.0f) : material.baseColor();
        checkColor(color);
        const bool flat = d.type == PrimitiveType::Plane || d.type == PrimitiveType::Disk;
        // 格式只承诺重建默认材质。外观被改成未描述的纹理/状态时，宁可拒绝也不静默丢失。
        if (!fileMaterial && (material.pbrParameters() || renderable.skin() || material.texture() || material.cullMode() != (flat ? CullMode::None : CullMode::Back) ||
            material.renderMode() != (color.a < 1 ? RenderMode::AlphaBlend : RenderMode::Opaque) ||
            !material.correctMirroredWinding() || material.shaderOutputsSrgb()))
        {
            throw std::invalid_argument("Cannot save modified builtin primitive material settings");
        }
        stream << "PRIMITIVE " << names.at(static_cast<std::size_t>(d.type)) << '\n'
            << "SIZE " << d.size.x << ' ' << d.size.y << ' ' << d.size.z << '\n'
            << "RADIUS " << d.radius << '\n' << "HEIGHT " << d.height << '\n'
            << "RADIAL_SEGMENTS " << d.radialSegments << '\n'
            << "LATITUDE_SEGMENTS " << d.latitudeSegments << '\n'
            << "COLOR " << color.r << ' ' << color.g << ' ' << color.b << ' ' << color.a << '\n';
    }
}
