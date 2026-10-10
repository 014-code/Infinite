#pragma once

#include "graphics/resources/ImageLoader.h"
#include "PbrResources.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <unordered_map>

class Shader;
class Texture;
class Mesh;
class Material;
class Model;

// ResourceManager负责缓存“可以按文件路径识别”的GPU资源。
//
// 目前管理Shader、Texture、Mesh、Material和Model：它们可由路径及加载配置识别，重复加载代价较高。
// Mesh和Material的程序创建入口仍然保留，ResourceManager只负责文件型资源缓存。
// 加载是同步操作，仅在主线程且拥有有效OpenGL上下文时调用；内部没有并发访问保护。
//
// ResourceManager必须在有效OpenGL上下文仍然存在时销毁。Application把它放在Window
// 成员之后声明，利用C++逆序析构规则让缓存先于Window释放。
class ResourceManager final
{
public:
    struct CacheEntryStats
    {
        // cached表示该类资源在ResourceManager中保存的键数量；
        // referenced表示其中仍被缓存外的其他资源或调用方引用的键数量。
        std::size_t cached = 0;
        std::size_t referenced = 0;
    };

    struct CacheStatistics
    {
        CacheEntryStats shaders;
        CacheEntryStats textures;
        CacheEntryStats meshes;
        CacheEntryStats materials;
        CacheEntryStats models;
    };

    ResourceManager() = default;
    ResourceManager(const ResourceManager &) = delete;
    ResourceManager &operator=(const ResourceManager &) = delete;
    ResourceManager(ResourceManager &&) = delete;
    ResourceManager &operator=(ResourceManager &&) = delete;

    // 第一次调用会读取、编译并缓存Shader；后续相同路径组合返回同一个shared_ptr。
    // 路径会转换为绝对、规范化形式，因此从不同相对路径写法访问同一文件也能命中缓存。
    std::shared_ptr<Shader> loadShader(
        const std::filesystem::path &vertexPath,
        const std::filesystem::path &fragmentPath);

    // 第一次调用会解码、上传并缓存Texture；后续相同路径和加载选项返回同一个shared_ptr。
    // 加载选项也参与缓存键，避免翻转方向不同的图片错误共享同一GPU资源。
    std::shared_ptr<Texture> loadTexture(
        const std::filesystem::path &path,
        const ImageLoadOptions &options = {});

    // 第一次调用会解析OBJ并上传Mesh；后续规范化路径命中同一个shared_ptr。
    std::shared_ptr<Mesh> loadMesh(const std::filesystem::path &path);

    // 第一次调用会解析材质文件，并通过本管理器加载其Shader和Texture。
    // 返回共享资源实例：读取属性是安全的，但修改材质会影响所有共享者。
    // 需要对象独立参数时，先对返回值调用Material::clone()；修改不会写回材质文件，也没有热重载。
    // 材质加载失败不会缓存半成品，但已成功加载的Shader/Texture依赖可以留在缓存中。
    std::shared_ptr<Material> loadMaterial(const std::filesystem::path &path);

    // 加载静态glTF模型。Shader由应用提供，因为glTF只描述材质参数，不携带本引擎的Shader源码。
    // 模型缓存键包含模型和Shader路径；同一模型可在不同预览Shader下各缓存一份。
    std::shared_ptr<Model> loadModel(const std::filesystem::path &modelPath,
        const std::filesystem::path &vertexShaderPath,
        const std::filesystem::path &fragmentShaderPath);
    // 显式核心PBR入口，使用引擎内置线性Shader；与旧预览入口分开缓存，不混淆颜色输出。
    std::shared_ptr<Model> loadPbrModel(const std::filesystem::path &modelPath);

    // 清空缓存，但不会销毁仍被Scene、Material或应用局部shared_ptr持有的资源。
    void clear() noexcept;

    // 只清理当前没有被缓存外引用的条目，适合SceneManager切换关卡后主动回收资源。
    // 清理顺序从上层Model开始，再处理Material、Mesh、Texture和Shader，保证依赖先释放。
    // 这个函数不会启动后台线程，也不会在加载过程中自动触发，生命周期完全由应用控制。
    void unloadUnused() noexcept;

    // 返回当前缓存快照，便于调试资源增长和验证场景切换后的清理结果。
    // referenced不等同于“有几个外部shared_ptr”，它按条目统计是否存在缓存以外的引用。
    CacheStatistics cacheStatistics() const noexcept;

    // 这些计数只表示缓存当前持有的条目数量，不代表应用外部shared_ptr的数量，
    // 方便调试缓存命中和测试资源释放时机。
    std::size_t shaderCount() const noexcept;
    std::size_t textureCount() const noexcept;
    std::size_t meshCount() const noexcept;
    std::size_t materialCount() const noexcept;
    std::size_t modelCount() const noexcept;

private:
    struct ShaderKey
    {
        std::filesystem::path vertexPath;
        std::filesystem::path fragmentPath;

        bool operator==(const ShaderKey &other) const noexcept;
    };

    struct ShaderKeyHash
    {
        std::size_t operator()(const ShaderKey &key) const noexcept;
    };

    struct TextureKey
    {
        std::filesystem::path path;
        ImageLoadOptions options;

        bool operator==(const TextureKey &other) const noexcept;
    };

    struct TextureKeyHash
    {
        std::size_t operator()(const TextureKey &key) const noexcept;
    };

    struct PathHash
    {
        std::size_t operator()(const std::filesystem::path &path) const noexcept;
    };

    struct ModelKey
    {
        std::filesystem::path modelPath;
        std::filesystem::path vertexShaderPath;
        std::filesystem::path fragmentShaderPath;

        bool operator==(const ModelKey &other) const noexcept;
    };

    struct ModelKeyHash
    {
        std::size_t operator()(const ModelKey &key) const noexcept;
    };

    std::unordered_map<ShaderKey, std::shared_ptr<Shader>, ShaderKeyHash> shaders_;
    std::unordered_map<TextureKey, std::shared_ptr<Texture>, TextureKeyHash> textures_;
    std::unordered_map<std::filesystem::path, std::shared_ptr<Mesh>, PathHash> meshes_;
    std::unordered_map<std::filesystem::path, std::shared_ptr<Material>, PathHash> materials_;
    std::unordered_map<ModelKey, std::shared_ptr<Model>, ModelKeyHash> models_;
    std::unordered_map<std::filesystem::path, std::shared_ptr<Model>, PathHash> pbrModels_;
    PbrResources pbrResources_;
};
