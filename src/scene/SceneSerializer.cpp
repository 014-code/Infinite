#include "SceneSerializer.h"

#include "scene/serialization/SceneBuilder.h"
#include "scene/serialization/SceneFileReader.h"
#include "scene/serialization/SceneFileWriter.h"

void SceneSerializer::save(const Scene &scene, const std::filesystem::path &path)
{
    scene_serialization_detail::SceneFileWriter::write(scene, path);
}

std::vector<ObjectId> SceneSerializer::load(Scene &scene, const std::filesystem::path &path)
{
    return loadImpl(scene, path, nullptr);
}

std::vector<ObjectId> SceneSerializer::load(Scene &scene, const std::filesystem::path &path,
    ResourceManager &resources)
{
    return loadImpl(scene, path, &resources);
}

std::vector<ObjectId> SceneSerializer::loadImpl(Scene &scene,
    const std::filesystem::path &path, ResourceManager *resources)
{
    // 先解析成纯数据，再由Builder创建临时Scene；任何失败都不会改变旧场景。
    const auto objects = scene_serialization_detail::SceneFileReader::read(path);
    return scene_serialization_detail::SceneBuilder::replace(scene, objects, resources);
}
