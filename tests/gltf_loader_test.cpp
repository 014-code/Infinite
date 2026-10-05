#include "TestSupport.h"
#include "assets/GltfLoader.h"
#include <cmath>
#include <iostream>

namespace
{
    // 不仅检查抛异常，也检查原始模型路径和出错对象编号，保证拆分后诊断仍可定位。
    void expectDiagnostic(const std::filesystem::path &path, const char *context)
    {
        try { GltfLoader::load(path); }
        catch (const std::runtime_error &error)
        {
            const std::string message = error.what();
            require(message.find(path.filename().u8string()) != std::string::npos &&
                message.find(context) != std::string::npos, "Missing import context: " + message);
            return;
        }
        throw std::runtime_error("Invalid fixture accepted: " + path.u8string());
    }

    void checkFlatNormalsAndBudget(const std::filesystem::path &path)
    {
        const auto model = GltfLoader::load(path);
        const auto &primitive = model.primitives.at(0);
        require(model.materials.size() == 1 && primitive.material == 0 &&
            model.materials[0].texture == -1, "Default material mapping changed");
        require(primitive.mesh.vertices.size() == 6 && primitive.mesh.indices.size() == 6,
            "Flat normals did not split indexed triangle vertices");
        for (std::size_t index = 0; index < 6; ++index)
        {
            require(primitive.mesh.indices[index] == index &&
                std::abs(primitive.mesh.vertices[index].normal.z - 1.0f) < .0001f,
                "Generated face normal or sequential index changed");
        }
        ModelLoadOptions options;
        // 旧的累计顶点计数语义保留：原始4个 + 展开6个，不能只计最终6个。
        options.maxVertices = 9;
        expectThrow<std::runtime_error>([&] { GltfLoader::load(path, options); },
            "Generated vertices escaped vertex limit");
        options.maxVertices = 10;
        GltfLoader::load(path, options);

        // 从小到大扫描小样本预算，必须分别观察到解析器和网格生成阶段拒绝申请。
        // 不写死cgltf结构体大小，所以Debug/Release及不同位数编译器均可运行。
        bool parserRejected = false, vertexRejected = false, flatRejected = false, loaded = false;
        for (std::size_t bytes = 64; bytes <= 65536; bytes += 64)
        {
            options.maxTotalBytes = bytes;
            try { GltfLoader::load(path, options); loaded = true; break; }
            catch (const std::runtime_error &error)
            {
                const std::string message = error.what();
                parserRejected |= message.find("cgltf parser allocations") != std::string::npos;
                vertexRejected |= message.find("vertex bytes") != std::string::npos;
                flatRejected |= message.find("generated normal vertex bytes") != std::string::npos;
            }
        }
        require(loaded && parserRejected && vertexRejected && flatRejected,
            "Shared byte budget did not cover parser, vertices and generated normals");

        // 精确找到成功边界，并检查差一个字节必定失败；失败不得污染下一次导入。
        std::size_t low = 0, high = options.maxTotalBytes;
        while (high - low > 1)
        {
            options.maxTotalBytes = low + (high - low) / 2;
            try { GltfLoader::load(path, options); high = options.maxTotalBytes; }
            catch (const std::runtime_error &) { low = options.maxTotalBytes; }
        }
        options.maxTotalBytes = high - 1;
        expectThrow<std::runtime_error>([&] { GltfLoader::load(path, options); }, "Byte boundary ignored");
        options.maxTotalBytes = high;
        GltfLoader::load(path, options);
    }
}

int main()
{
    try
    {
        const std::filesystem::path directory = "tests/fixtures/gltf";
        for (const auto *name : {"quad.gltf", "quad.glb", "embedded.gltf", "indices-5121.glb", "indices-5125.glb", "nonindexed.glb"})
        {
            const auto model = GltfLoader::load(directory / name);
            require(model.nodes.size() == 2 && model.nodes[1].parent == 0, "Hierarchy lost");
            const auto &mesh = model.primitives.at(0).mesh;
            require(mesh.indices.size() == 6 && mesh.vertices[0].uv.y == 1 &&
                std::abs(mesh.vertices[0].position.x + .6f) < .0001f, "Accessor/stride/UV read failed");
            require(model.images.size() == 1 && model.materials[0].texture == 0 && !model.warnings.empty(), "Material/image/warning lost");
        }
        const auto multiple = GltfLoader::load(directory / "multi.gltf");
        require(multiple.nodes[1].primitives.size() == 2 && multiple.primitives[1].material == 1, "Primitive material mapping lost");
        const auto matrix = GltfLoader::load(directory / "matrix.gltf");
        require(matrix.nodes[0].scale.x == -2 && matrix.nodes[0].position.y == 2, "Matrix TRS changed");
        const auto rotation = GltfLoader::load(directory / "quaternion.gltf").nodes[0].rotation;
        require(std::abs(rotation.z - std::sin(.3f)) < .0001f, "Quaternion component order changed");
        for (const auto *name : {"bad-offset", "bad-index", "blend", "mask", "extension", "cycle", "shear", "morph", "uv1"})
        {
            expectThrow<std::runtime_error>([&] { GltfLoader::load(directory / (std::string(name) + ".gltf")); }, name);
        }
        ModelLoadOptions limits;
        limits.maxVertices = 3;
        expectThrow<std::runtime_error>([&] { GltfLoader::load(directory / "quad.glb", limits); }, "Vertex budget ignored");
        limits = {}; limits.maxDepth = 1;
        expectThrow<std::runtime_error>([&] { GltfLoader::load(directory / "quad.glb", limits); }, "Depth budget ignored");
        limits = {}; limits.maxTotalBytes = 10;
        expectThrow<std::runtime_error>([&] { GltfLoader::load(directory / "quad.glb", limits); }, "Byte budget ignored");
        checkFlatNormalsAndBudget(directory / "flat-normals.gltf");
        expectDiagnostic(directory / "shear.gltf", "Node[0]");
        expectDiagnostic(directory / "blend.gltf", "Material[0]");
        expectDiagnostic(directory / "uv1.gltf", "Material[0]");
        expectDiagnostic(directory / "bad-offset.gltf", "Accessor[");
        expectDiagnostic(directory / "bad-image-view.gltf", "BufferView[0]");
        expectDiagnostic(directory / "bad-base64.gltf", "Buffer[0]");
        expectDiagnostic(directory / "bad-uri.gltf", "Buffer[0]");
        expectDiagnostic(directory / "bad-image-format.gltf", "Image[0]");
        std::cout << "glTF CPU import passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
