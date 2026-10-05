#include "Renderable.h"
#include "resources/ResourceManager.h"

#include <stdexcept>
#include <utility>

void Renderable::set(const Mesh &mesh, const Material &material)
{
    skin_.reset();
    // 任意手工绑定都使旧的文件来源失效，否则保存时可能写出不对应当前资源的路径。
    primitive_.reset();
    builtinPrimitiveMaterial_ = false;
    meshPath_.clear();
    materialPath_.clear();
    // 重新绑定不同资源时，先释放旧的持有关系；如果仍然绑定同一个对象，
    // 则保留已有shared_ptr，避免“持有模式”意外退化成悬空借用。
    if (&mesh != mesh_)
    {
        ownedMesh_.reset();
    }
    if (&material != material_)
    {
        ownedMaterial_.reset();
    }
    mesh_ = &mesh;
    material_ = &material;
}

void Renderable::set(
    std::shared_ptr<const Mesh> mesh,
    std::shared_ptr<const Material> material)
{
    if (!mesh || !material)
    {
        throw std::invalid_argument("Renderable requires both Mesh and Material");
    }

    primitive_.reset();
    builtinPrimitiveMaterial_ = false;
    meshPath_.clear();
    materialPath_.clear();

    // 先接管shared_ptr，再从它们取得裸指针。这样组件始终保持“指针和所有权”一致，
    // 不会出现裸指针已经更新但shared_ptr还指向旧资源的中间状态。
    ownedMesh_ = std::move(mesh);
    skin_.reset();
    ownedMaterial_ = std::move(material);
    mesh_ = ownedMesh_.get();
    material_ = ownedMaterial_.get();
}

void Renderable::clear()
{
    skin_.reset();
    primitive_.reset();
    builtinPrimitiveMaterial_ = false;
    meshPath_.clear();
    materialPath_.clear();
    // 先清空裸指针，再释放shared_ptr，逻辑上明确表示组件已经不再可绘制。
    mesh_ = nullptr;
    material_ = nullptr;
    ownedMesh_.reset();
    ownedMaterial_.reset();
}

void Renderable::setFromFiles(ResourceManager &resources,
    const std::filesystem::path &meshPath, const std::filesystem::path &materialPath)
{
    if (meshPath.empty() || materialPath.empty())
    {
        throw std::invalid_argument("Renderable requires both asset paths");
    }
    auto meshSource = std::filesystem::absolute(meshPath).lexically_normal();
    auto materialSource = std::filesystem::absolute(materialPath).lexically_normal();
    // 先加载两份资源，成功后才调用set替换当前绑定。
    // 因此材质文件损坏或Shader编译失败时，当前Renderable仍保持原状态；
    // 已成功加载的Mesh可能留在ResourceManager缓存中，这是安全的缓存副作用。
    auto mesh = resources.loadMesh(meshSource);
    auto material = resources.loadMaterial(materialSource);
    set(std::move(mesh), std::move(material));
    // swap不分配内存，资源加载失败或路径构造失败时仍保留原绑定。
    meshPath_.swap(meshSource);
    materialPath_.swap(materialSource);
}

const std::filesystem::path &Renderable::meshPath() const noexcept { return meshPath_; }
const std::filesystem::path &Renderable::materialPath() const noexcept { return materialPath_; }

bool Renderable::isBound() const noexcept
{
    return mesh_ != nullptr && material_ != nullptr;
}

const Mesh *Renderable::mesh() const noexcept
{
    return mesh_;
}

const Material *Renderable::material() const noexcept
{
    return material_;
}

const glm::vec3 &Renderable::sortOrigin() const noexcept
{
    return sortOrigin_;
}

void Renderable::setSortOrigin(const glm::vec3 &origin) noexcept
{
    sortOrigin_ = origin;
}
