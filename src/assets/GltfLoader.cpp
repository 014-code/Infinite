#include "GltfLoader.h"

#include "gltf/GltfFileReader.h"
#include "gltf/GltfMaterialDecoder.h"
#include "gltf/GltfMeshDecoder.h"
#include "gltf/GltfNodeDecoder.h"
#include "gltf/GltfValidator.h"
#include "gltf/GltfAnimationDecoder.h"
#include "gltf/GltfSkinDecoder.h"

#include <filesystem>
#include <stdexcept>

namespace
{
    // 这里故意只保留阶段顺序。每个阶段的边界检查和数据转换在自己的模块中完成，
    // 以后扩展PBR、动画或压缩网格时，可以替换对应解码器而不继续膨胀入口文件。
    ModelData importModel(const std::filesystem::path &path, const ModelLoadOptions &options)
    {
        gltf::GltfImportContext context(path, options);

        gltf::GltfFileReader::read(context);
        gltf::GltfValidator::validate(context);
        gltf::GltfFileReader::readImages(context);
        gltf::GltfMaterialDecoder::decode(context);
        gltf::GltfMeshDecoder::decode(context);
        gltf::GltfNodeDecoder::decode(context);
        gltf::GltfSkinDecoder::decode(context);
        gltf::GltfAnimationDecoder::decode(context);

        return std::move(context).takeResult();
    }
}

ModelData GltfLoader::load(const std::filesystem::path &path, const ModelLoadOptions &options)
{
    try
    {
        // 统一成绝对规范路径，保证外部buffer、图片和错误信息使用同一份路径基准。
        return importModel(std::filesystem::absolute(path).lexically_normal(), options);
    }
    catch (const std::exception &error)
    {
        throw std::runtime_error("Failed to import glTF '" + path.u8string() + "': " + error.what());
    }
}
