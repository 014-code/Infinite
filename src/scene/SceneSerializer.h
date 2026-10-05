#pragma once

#include <filesystem>
#include <vector>
#include "GameObject.h"

class Scene;
class ResourceManager;

// SceneSerializer负责保存和恢复Scene中的结构化数据。
//
// 版本4增加基础几何描述和内置材质颜色；仍兼容版本1/2/3文件。
// C++更新回调不保存。无可重建描述的运行时网格、无文件来源的自定义材质仍明确报错。
// 加载含基础几何的场景要求目标Scene绑定PrimitiveResources，且当前OpenGL上下文有效。
// 当前格式不保存光照；加载成功或失败均保留目标Scene原有SceneLighting。
class SceneSerializer final
{
public:
    // 保存为可直接阅读的版本化文本文件。目标文件所在目录必须已经存在。
    // 完整写入同目录临时文件并检查刷新/关闭后才替换；失败不会主动截断或删除旧档。
    // 不保证断电恢复、旧文件权限/元数据保留或并发写入互斥；普通本地文件路径为支持范围。
    static void save(const Scene &scene, const std::filesystem::path &path);

    // 先构建临时场景，全部成功才替换目标；返回按文件顺序排列的新ID，供应用绑定行为。
    // 文件Mesh仍要求显式传resources；基础几何使用Scene绑定的服务，文件材质可借用其ResourceManager。
    // 失败时旧Scene保持不变；此前已成功加载的依赖可能留在resources缓存中供下次复用。
    static std::vector<ObjectId> load(Scene &scene, const std::filesystem::path &path);
    static std::vector<ObjectId> load(Scene &scene, const std::filesystem::path &path,
        ResourceManager &resources);

private:
    static std::vector<ObjectId> loadImpl(Scene &scene, const std::filesystem::path &path,
        ResourceManager *resources);
};
