#include "ModelTextureUploader.h"
#include "graphics/resources/ImageLoader.h"
#include "graphics/resources/Texture.h"
#include <map>
#include <tuple>

ModelTextureSet uploadModelTextures(const ModelData &data, const std::filesystem::path &source)
{
    std::vector<ImageData> images;
    std::size_t remaining = 256 * 1024 * 1024;
    for (std::size_t i = 0; i < data.images.size(); ++i)
    {
        ImageLoadOptions options;
        options.flipVertically = false;
        options.maxDecodedBytes = remaining;
        auto image = ImageLoader::loadMemory(data.images[i], options, source.u8string() + " image " + std::to_string(i));
        remaining -= image.pixels.size();
        images.push_back(std::move(image));
    }
    ModelTextureSet result;
    result.colors.resize(data.textures.size()); result.linear.resize(data.textures.size());
    using Key = std::tuple<std::size_t, int, int, int, int, bool>;
    std::map<Key, std::shared_ptr<Texture>> cache;
    const auto upload = [&](int index, bool srgb)
    {
        if (index < 0) { return; }
        const auto &descriptor = data.textures.at(static_cast<std::size_t>(index));
        const Key key{descriptor.image, descriptor.minFilter, descriptor.magFilter, descriptor.wrapS, descriptor.wrapT, srgb};
        auto &texture = cache[key];
        if (!texture)
        {
            Texture::SamplingOptions options;
            options.minFilter = descriptor.minFilter; options.magFilter = descriptor.magFilter;
            options.wrapS = descriptor.wrapS; options.wrapT = descriptor.wrapT; options.srgb = srgb;
            texture = std::make_shared<Texture>(images.at(descriptor.image), options);
        }
        (srgb ? result.colors : result.linear).at(static_cast<std::size_t>(index)) = texture;
    };
    for (const auto &material : data.materials)
    {
        upload(material.texture, true);
        if (data.pbrMaterials)
        {
            for (std::size_t slot = 0; slot < material.pbrTextures.size(); ++slot)
            { upload(material.pbrTextures[slot], slot == 3); }
        }
    }
    return result;
}
