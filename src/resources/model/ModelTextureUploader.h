#pragma once
#include "assets/ModelData.h"
#include <memory>
#include <filesystem>

class Texture;
struct ModelTextureSet
{
    // 按glTF texture索引寻址，同一个索引的颜色/数据用不同GPU格式；未使用的槽为空。
    std::vector<std::shared_ptr<Texture>> colors;
    std::vector<std::shared_ptr<Texture>> linear;
};

// CPU图片只解码一次，GPU纹理按image+sampler+颜色空间复用。仅在有效GL上下文使用。
ModelTextureSet uploadModelTextures(const ModelData &data, const std::filesystem::path &source);
