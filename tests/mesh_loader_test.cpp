#include "TestSupport.h"
#include "assets/MeshLoader.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void writeText(const std::filesystem::path &path, const char *text)
    {
        std::ofstream file(path);
        if (!file) { throw std::runtime_error("Could not create OBJ fixture"); }
        file << text;
    }

    void requireNear(float actual, float expected, const char *message)
    {
        require(std::abs(actual - expected) < 0.0001f, message);
    }
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected mesh loader test output directory");
        const std::filesystem::path directory = argv[1];
        std::filesystem::create_directories(directory);

        const auto quadPath = directory / "quad.obj";
        writeText(quadPath,
            "# Quad without normals: the loader must generate face normals.\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 1 1 0\n"
            "v 0 1 0\n"
            "vt 0 0\n"
            "vt 1 0\n"
            "vt 1 1\n"
            "vt 0 1\n"
            "f 1/1 2/2 3/3 4/4\n");
        const MeshData quad = MeshLoader::loadObj(quadPath);
        require(quad.vertices.size() == 4 && quad.indices.size() == 6,
            "Quad was not triangulated or deduplicated correctly");
        requireNear(quad.vertices[0].normal.z, 1.0f, "Generated normal points in the wrong direction");
        requireNear(quad.vertices[2].uv.x, 1.0f, "UV was not loaded");
        require(quad.vertices[0].color == glm::vec3(1.0f), "OBJ default color was not white");

        const auto explicitPath = directory / "explicit.obj";
        writeText(explicitPath,
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "vn 0 0 -1\n"
            "f -3//-1 -2//-1 -1//-1\n");
        const MeshData explicitNormals = MeshLoader::loadObj(explicitPath);
        require(explicitNormals.vertices.size() == 3 && explicitNormals.indices.size() == 3,
            "Negative OBJ indices were not resolved");
        requireNear(explicitNormals.vertices[0].normal.z, -1.0f,
            "Explicit OBJ normal was replaced unexpectedly");

        const auto invalidPath = directory / "invalid.obj";
        writeText(invalidPath, "v 0 0 0\nv 1 0 0\nf 1 2 3\n");
        expectThrow<std::runtime_error>([&] { MeshLoader::loadObj(invalidPath); },
            "Out-of-range OBJ index was accepted");

        const auto degeneratePath = directory / "degenerate.obj";
        writeText(degeneratePath, "v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n");
        expectThrow<std::runtime_error>([&] { MeshLoader::loadObj(degeneratePath); },
            "Degenerate OBJ face was accepted");

        std::cout << "Mesh loader passed: OBJ faces, UVs, normals, negative indices and validation\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Mesh loader failed: " << error.what() << '\n';
        return 1;
    }
}
