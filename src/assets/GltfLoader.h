#pragma once

#include "ModelData.h"
#include <filesystem>

struct ModelLoadOptions
{
    bool pbrMaterials = false; // 显式选择完整核心MR材质数据；默认保留旧基础颜色预览的拒绝策略。
    std::size_t maxFileBytes = 64 * 1024 * 1024;
    // 文件、解析器、输出数组和生成法线共用累计字节预算，释放不返还。
    // 不含分配器开销/诊断字符串/调用栈；不是进程内存峰值，图片解码由后续阶段限制。
    std::size_t maxTotalBytes = 256 * 1024 * 1024;
    std::size_t maxVertices = 2000000;
    std::size_t maxIndices = 6000000;
    std::size_t maxNodes = 10000;
    std::size_t maxDepth = 128;
    std::size_t maxAnimationKeys = 2000000; // 所有通道合计，连同字节预算一起检查。
};

// 同步、纯CPU的glTF 2.0导入器；PBR材质由选项开启，STEP/LINEAR节点动画独立导入。
// 读取默认scene，未指定默认scene时使用第一个；不修改文件，不访问网络。
// PBR入口另支持每顶点4权重蒙皮；形变、三次插值、稀疏accessor和未知必需扩展明确报错。
// 预览模式仍拒绝透明材质和蒙皮，不把旧自定义Shader误认为支持顶点变形。
class GltfLoader final
{
public:
    static ModelData load(const std::filesystem::path &path, const ModelLoadOptions &options = {});
};
