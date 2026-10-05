#include "Model.h"
#include "model/ModelTextureUploader.h"

#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"

Model::Model(const std::filesystem::path &sourcePath, const ModelData &data,
    std::shared_ptr<Shader> shader, const ModelTextureSet &textures)
    : sourcePath_(sourcePath), animations_(data.animations), skins_(data.skins)
{
    for (const auto &skin : skins_)
    {
        if (skin.joints.size() > shader->matrixArrayCapacity("jointMatrices[0]"))
        { throw std::runtime_error("Model skin exceeds the current device/shader joint capacity"); }
    }
    std::vector<std::shared_ptr<Material>> materials;
    materials.reserve(data.materials.size());
    for (const auto &materialData : data.materials)
    {
        std::shared_ptr<Texture> texture;
        if (materialData.texture >= 0)
        {
            texture = textures.colors.at(static_cast<std::size_t>(materialData.texture));
        }
        auto material = std::make_shared<Material>(shader, materialData.baseColor, texture);
        // 双面材质关闭背面剔除；PBR入口另按glTF设置Opaque/MASK/BLEND。
        material->setCullMode(materialData.doubleSided ? CullMode::None : CullMode::Back);
        material->setCorrectMirroredWinding(true);
        material->setShaderOutputsSrgb(!data.pbrMaterials);
        if (data.pbrMaterials)
        {
            material->setPbrParameters(materialData.pbr);
            material->setRenderMode(materialData.alphaMode == ModelAlphaMode::Blend ? RenderMode::AlphaBlend :
                materialData.alphaMode == ModelAlphaMode::Mask ? RenderMode::AlphaMask : RenderMode::Opaque);
            for (std::size_t slot = 0; slot < materialData.pbrTextures.size(); ++slot)
            {
                const int index = materialData.pbrTextures[slot];
                if (index >= 0) { material->setPbrTexture(static_cast<PbrTextureSlot>(slot),
                    (slot == 3 ? textures.colors : textures.linear).at(static_cast<std::size_t>(index))); }
            }
        }
        materials.push_back(std::move(material));
    }

    primitives_.reserve(data.primitives.size());
    for (const auto &primitiveData : data.primitives)
    {
        Primitive primitive;
        primitive.mesh = std::make_shared<Mesh>(primitiveData.mesh, primitiveData.skinVertices);
        primitive.material = materials.at(primitiveData.material);
        primitives_.push_back(std::move(primitive));
    }

    nodes_.reserve(data.nodes.size());
    for (const auto &nodeData : data.nodes)
    {
        nodes_.push_back({nodeData.name, nodeData.parent, nodeData.position,
            nodeData.rotation, nodeData.scale, nodeData.primitives, nodeData.skin});
    }
}

const std::filesystem::path &Model::sourcePath() const noexcept { return sourcePath_; }
const std::vector<Model::Primitive> &Model::primitives() const noexcept { return primitives_; }
const std::vector<Model::Node> &Model::nodes() const noexcept { return nodes_; }
