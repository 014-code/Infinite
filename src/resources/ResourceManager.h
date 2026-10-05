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
    // 返回共享实例：修改材质会影响所有共享者，不会自动写回材质文件，也没有热重载。
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
