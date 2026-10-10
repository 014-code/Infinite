#include "ResourceManager.h"

#include "assets/MaterialLoader.h"
#include "assets/GltfLoader.h"
#include "assets/MeshLoader.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"
#include "Model.h"
#include "model/ModelTextureUploader.h"
#include "core/Log.h"

#include <functional>
#include <stdexcept>
#include <system_error>

namespace
{
    template<class Cache>
    ResourceManager::CacheEntryStats collectCacheStats(const Cache &cache) noexcept
    {
        ResourceManager::CacheEntryStats result;
        result.cached = cache.size();
        for (const auto &entry : cache)
        {
            if (entry.second.use_count() > 1)
            {
                ++result.referenced;
            }
        }
        return result;
    }

    template<class Cache>
    void unloadUnusedEntries(Cache &cache) noexcept
    {
        for (auto entry = cache.begin(); entry != cache.end();)
        {
            if (entry->second.use_count() == 1)
            {
                entry = cache.erase(entry);
            }
            else
            {
                ++entry;
            }
        }
    }

    ResourceManager::CacheEntryStats combineCacheStats(
        const ResourceManager::CacheEntryStats &first,
        const ResourceManager::CacheEntryStats &second) noexcept
    {
        return {first.cached + second.cached, first.referenced + second.referenced};
    }

    // 缓存键不直接使用调用者传入的相对路径，否则从不同工作目录或使用
    // "assets/../assets/a.ppm" 这样的写法会生成重复的GPU资源。
    std::filesystem::path normalizedPath(const std::filesystem::path &path)
    {
        if (path.empty())
        {
            throw std::invalid_argument("Resource path must not be empty");
        }

        std::error_code error;
        const auto absolute = std::filesystem::absolute(path, error);
        if (error)
        {
            throw std::runtime_error("Failed to normalize resource path: " + error.message());
        }
        return absolute.lexically_normal();
    }

    void combineHash(std::size_t &seed, std::size_t value) noexcept
    {
        // 这是常见的hash组合写法；它只用于查找缓存，不参与资源内容校验。
        seed ^= value + static_cast<std::size_t>(0x9e3779b9u) + (seed << 6) + (seed >> 2);
    }

    std::size_t pathHash(const std::filesystem::path &path) noexcept
    {
        return std::hash<std::string>{}(path.u8string());
    }

    bool sameOptions(const ImageLoadOptions &left, const ImageLoadOptions &right) noexcept
    {
        return left.maxDimension == right.maxDimension &&
            left.maxFileBytes == right.maxFileBytes &&
            left.maxDecodedBytes == right.maxDecodedBytes &&
            left.flipVertically == right.flipVertically;
    }
}

bool ResourceManager::ShaderKey::operator==(const ShaderKey &other) const noexcept
{
    return vertexPath == other.vertexPath && fragmentPath == other.fragmentPath;
}

std::size_t ResourceManager::ShaderKeyHash::operator()(const ShaderKey &key) const noexcept
{
    std::size_t result = pathHash(key.vertexPath);
    combineHash(result, pathHash(key.fragmentPath));
    return result;
}

bool ResourceManager::TextureKey::operator==(const TextureKey &other) const noexcept
{
    return path == other.path && sameOptions(options, other.options);
}

std::size_t ResourceManager::TextureKeyHash::operator()(const TextureKey &key) const noexcept
{
    std::size_t result = pathHash(key.path);
    combineHash(result, std::hash<int>{}(key.options.maxDimension));
    combineHash(result, std::hash<std::size_t>{}(key.options.maxFileBytes));
    combineHash(result, std::hash<std::size_t>{}(key.options.maxDecodedBytes));
    combineHash(result, std::hash<bool>{}(key.options.flipVertically));
    return result;
}

std::size_t ResourceManager::PathHash::operator()(const std::filesystem::path &path) const noexcept
{
    return pathHash(path);
}

bool ResourceManager::ModelKey::operator==(const ModelKey &other) const noexcept
{
    return modelPath == other.modelPath && vertexShaderPath == other.vertexShaderPath &&
        fragmentShaderPath == other.fragmentShaderPath;
}

std::size_t ResourceManager::ModelKeyHash::operator()(const ModelKey &key) const noexcept
{
    std::size_t result = pathHash(key.modelPath);
    combineHash(result, pathHash(key.vertexShaderPath));
    combineHash(result, pathHash(key.fragmentShaderPath));
    return result;
}

std::shared_ptr<Shader> ResourceManager::loadShader(
    const std::filesystem::path &vertexPath,
    const std::filesystem::path &fragmentPath)
{
    const ShaderKey key{normalizedPath(vertexPath), normalizedPath(fragmentPath)};
    const auto found = shaders_.find(key);
    if (found != shaders_.end())
    {
        // 缓存命中时不重新读文件，也不重新创建OpenGL程序。
        return found->second;
    }

    // 先构造成功，再放入缓存。构造失败时不会留下半初始化的缓存条目。
    auto shader = std::make_shared<Shader>(key.vertexPath, key.fragmentPath);
    shaders_.emplace(key, shader);
    return shader;
}

std::shared_ptr<Texture> ResourceManager::loadTexture(
    const std::filesystem::path &path,
    const ImageLoadOptions &options)
{
    const TextureKey key{normalizedPath(path), options};
    const auto found = textures_.find(key);
    if (found != textures_.end())
    {
        // 翻转方向和解码限制属于缓存键的一部分，命中时可以安全复用原纹理。
        return found->second;
    }

    auto texture = std::make_shared<Texture>(key.path, options);
    textures_.emplace(key, texture);
    return texture;
}

std::shared_ptr<Mesh> ResourceManager::loadMesh(const std::filesystem::path &path)
{
    const std::filesystem::path key = normalizedPath(path);
    const auto found = meshes_.find(key);
    if (found != meshes_.end())
    {
        return found->second;
    }

    // 先完整解析并上传成功，再放入缓存；损坏OBJ不会留下半初始化条目。
    auto mesh = std::make_shared<Mesh>(MeshLoader::loadObj(key));
    meshes_.emplace(key, mesh);
    return mesh;
}

std::shared_ptr<Material> ResourceManager::loadMaterial(const std::filesystem::path &path)
{
    // 材质文件本身只是配置；加载Material时会递归请求它引用的Shader和Texture，
    // 因此多个Material可以自然共享同一份底层GPU资源。
    const std::filesystem::path key = normalizedPath(path);
    const auto found = materials_.find(key);
    if (found != materials_.end())
    {
        return found->second;
    }

    const MaterialData data = MaterialLoader::load(key);
    auto shader = loadShader(data.vertexShaderPath, data.fragmentShaderPath);
    std::shared_ptr<Texture> texture;
    if (data.hasTexture)
    {
        texture = loadTexture(data.texturePath);
    }
    auto material = std::make_shared<Material>(shader, data.baseColor, texture);
    // Material保存的是shared_ptr，清空ResourceManager缓存后，外部仍在使用的材质不会失效。
    material->setRenderMode(data.renderMode);
    material->setCullMode(data.cullMode);
    materials_.emplace(key, material);
    return material;
}

std::shared_ptr<Model> ResourceManager::loadModel(
    const std::filesystem::path &modelPath,
    const std::filesystem::path &vertexShaderPath,
    const std::filesystem::path &fragmentShaderPath)
{
    const ModelKey key{normalizedPath(modelPath), normalizedPath(vertexShaderPath),
        normalizedPath(fragmentShaderPath)};
    const auto found = models_.find(key);
    if (found != models_.end()) { return found->second; }

    // 解析先于GPU资源构建；失败时不会把半成品写入模型缓存。
    const ModelData data = GltfLoader::load(key.modelPath);
    for (const auto &warning : data.warnings) { LOG_WARN(key.modelPath.u8string() + ": " + warning); }
    auto shader = loadShader(key.vertexShaderPath, key.fragmentShaderPath);
    auto textures = uploadModelTextures(data, key.modelPath);
    // 私有构造只能由ResourceManager调用，不能通过不具备友元权限的make_shared间接调用。
    auto model = std::shared_ptr<Model>(new Model(key.modelPath, data, std::move(shader), std::move(textures)));
    models_.emplace(key, model);
    return model;
}

std::shared_ptr<Model> ResourceManager::loadPbrModel(const std::filesystem::path &modelPath)
{
    const auto key = normalizedPath(modelPath);
    const auto found = pbrModels_.find(key);
    if (found != pbrModels_.end()) { return found->second; }
    ModelLoadOptions options;
    options.pbrMaterials = true;
    const auto data = GltfLoader::load(key, options);
    for (const auto &warning : data.warnings) { LOG_WARN(key.u8string() + ": " + warning); }
    auto textures = uploadModelTextures(data, key);
    auto model = std::shared_ptr<Model>(new Model(key, data, pbrResources_.shader(), textures));
    pbrModels_.emplace(key, model);
    return model;
}

void ResourceManager::clear() noexcept
{
    models_.clear();
    pbrModels_.clear();
    pbrResources_.clear();
    // Material内部可能持有Shader和Texture，因此先清Material，再清底层资源缓存。
    materials_.clear();
    meshes_.clear();
    textures_.clear();
    shaders_.clear();
}

void ResourceManager::unloadUnused() noexcept
{
    // Model内部保存Mesh、Material和纹理；必须先释放不再使用的Model，
    // 后面的缓存才有机会观察到这些依赖已经没有额外引用。
    unloadUnusedEntries(models_);
    unloadUnusedEntries(pbrModels_);
    pbrResources_.unloadUnused();

    unloadUnusedEntries(materials_);
    unloadUnusedEntries(meshes_);
    unloadUnusedEntries(textures_);
    unloadUnusedEntries(shaders_);
}

ResourceManager::CacheStatistics ResourceManager::cacheStatistics() const noexcept
{
    CacheStatistics result;
    result.shaders = collectCacheStats(shaders_);
    result.textures = collectCacheStats(textures_);
    result.meshes = collectCacheStats(meshes_);
    result.materials = collectCacheStats(materials_);
    result.models = combineCacheStats(collectCacheStats(models_), collectCacheStats(pbrModels_));
    return result;
}

std::size_t ResourceManager::shaderCount() const noexcept
{
    return shaders_.size();
}

std::size_t ResourceManager::textureCount() const noexcept
{
    return textures_.size();
}

std::size_t ResourceManager::meshCount() const noexcept
{
    return meshes_.size();
}

std::size_t ResourceManager::materialCount() const noexcept
{
    return materials_.size();
}

std::size_t ResourceManager::modelCount() const noexcept
{
    return models_.size() + pbrModels_.size();
}
