#include "TestSupport.h"

#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Material.h"
#include "platform/Window.h"
#include "resources/ResourceManager.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

int main()
{
    try
    {
        // Shader和Texture都创建OpenGL对象，因此测试必须先创建有效上下文。
        Window window(128, 128, "Resource Manager Test", false);
        ResourceManager resources;

        const std::filesystem::path shaderDirectory = "examples/scene_objects/shaders";
        const std::filesystem::path texturePath = "examples/textured_quad/assets/checker.ppm";

        auto shader = resources.loadShader(
            shaderDirectory / "scene.vert",
            shaderDirectory / "scene.frag");
        // 加入“./”只是改变书写方式，规范化后应当命中同一缓存项。
        auto sameShader = resources.loadShader(
            shaderDirectory / "." / "scene.vert",
            shaderDirectory / "." / "scene.frag");
        require(shader == sameShader, "Equivalent Shader paths did not hit the cache");
        require(resources.shaderCount() == 1, "Shader cache contains duplicate entries");

        auto texture = resources.loadTexture(texturePath);
        auto sameTexture = resources.loadTexture(texturePath.parent_path() / "." / texturePath.filename());
        require(texture == sameTexture, "Equivalent Texture paths did not hit the cache");
        require(resources.textureCount() == 1, "Texture cache contains duplicate entries");

        // 翻转选项会改变CPU像素排列，不能错误地复用默认选项创建的纹理。
        ImageLoadOptions differentOptions;
        differentOptions.flipVertically = false;
        auto differentTexture = resources.loadTexture(texturePath, differentOptions);
        require(differentTexture != texture, "Different Texture options reused one resource");
        require(resources.textureCount() == 2, "Texture options were not included in the cache key");
        const auto initialStats = resources.cacheStatistics();
        require(initialStats.shaders.cached == 1 && initialStats.shaders.referenced == 1,
            "Shader cache statistics are wrong");
        require(initialStats.textures.cached == 2 && initialStats.textures.referenced == 2,
            "Texture cache statistics are wrong");

        // 清缓存只释放缓存自己的引用；调用者仍持有的资源必须继续有效。
        std::weak_ptr<Shader> shaderLifetime = shader;
        std::weak_ptr<Texture> textureLifetime = texture;
        resources.clear();
        require(resources.shaderCount() == 0 && resources.textureCount() == 0,
            "Resource cache was not cleared");
        require(!shaderLifetime.expired() && !textureLifetime.expired(),
            "Clearing cache destroyed externally owned resources");

        shader.reset();
        sameShader.reset();
        texture.reset();
        sameTexture.reset();
        differentTexture.reset();
        require(shaderLifetime.expired() && textureLifetime.expired(),
            "Resources remained alive after cache and caller references were released");

        // unloadUnused只释放没有调用方持有的缓存项；调用方仍持有时不能误删资源。
        auto retainedShader = resources.loadShader(
            shaderDirectory / "scene.vert", shaderDirectory / "scene.frag");
        auto transientTexture = resources.loadTexture(texturePath);
        std::weak_ptr<Shader> transientShaderLifetime = retainedShader;
        std::weak_ptr<Texture> transientTextureLifetime = transientTexture;
        resources.unloadUnused();
        require(resources.shaderCount() == 1 && resources.textureCount() == 1,
            "unloadUnused removed resources still held by the caller");
        retainedShader.reset();
        transientTexture.reset();
        resources.unloadUnused();
        require(resources.shaderCount() == 0 && resources.textureCount() == 0,
            "unloadUnused kept unreferenced resources");
        require(transientShaderLifetime.expired() && transientTextureLifetime.expired(),
            "unloadUnused did not release unreferenced GPU resources");

        auto mesh = resources.loadMesh("tests/fixtures/assets/quad.obj");
        auto material = resources.loadMaterial("tests/fixtures/assets/test.material");
        require(mesh == resources.loadMesh("tests/fixtures/assets/./quad.obj") &&
            material == resources.loadMaterial("tests/fixtures/assets/./test.material"),
            "Mesh or Material cache miss");
        require(mesh->indexCount() == 6 && material->baseColor().g == 0.5f &&
            material->cullMode() == CullMode::Back && material->texture() != nullptr,
            "File resources were not composed correctly");
        std::weak_ptr<Mesh> meshLifetime = mesh;
        std::weak_ptr<Material> materialLifetime = material;
        resources.clear();
        require(resources.meshCount() == 0 && resources.materialCount() == 0 &&
            !meshLifetime.expired() && !materialLifetime.expired(), "Shared file resources lost");
        // 清缓存后，外部材质仍持有Shader/Texture，能够真正应用到当前上下文。
        material->use();
        mesh->draw();
        require(glGetError() == GL_NO_ERROR, "Resources failed after cache clear");
        mesh.reset();
        material.reset();
        require(meshLifetime.expired() && materialLifetime.expired(), "File resources leaked");
        expectThrow<std::runtime_error>([&] { resources.loadMesh("missing.obj"); }, "Missing mesh accepted");
        expectThrow<std::runtime_error>([&] { resources.loadMaterial("missing.material"); }, "Missing material accepted");
        require(resources.meshCount() == 0 && resources.materialCount() == 0, "Failed loads cached");

        expectThrow<std::invalid_argument>([&]
        {
            resources.loadShader({}, shaderDirectory / "scene.frag");
        }, "Empty Shader path accepted");
        expectThrow<std::runtime_error>([&]
        {
            resources.loadTexture("tests/fixtures/images/missing.ppm");
        }, "Missing Texture file accepted");

        std::cout << "ResourceManager cache and lifetimes passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
