#include "graphics/rendering/Renderer.h"

#include <GL/glew.h>

#include "graphics/camera/Camera.h"
#include "graphics/lighting/Lighting.h"
#include "graphics/lighting/LightingUniforms.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Shader.h"
#include "math/Transform.h"
#include "shadows/DirectionalShadowMap.h"

#include <stdexcept>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace
{
    // 仅在本次绘制调用中存活的快照，不放进Transform/Scene中长期缓存。
    // position/scale允许直接修改，因此跨帧缓存需要额外失效机制；本次优化不引入该复杂度。
    struct PreparedDraw
    {
        const Mesh *mesh;
        const Material *material;
        glm::mat4 model;
        glm::mat3 normal;
        bool needsNormal;
        float depth = 0.0f;
        const std::vector<glm::mat4> *skin = nullptr;
    };

    PreparedDraw prepareDraw(const Mesh &mesh, const Material &material, const Transform &transform)
    {
        const auto model = transform.worldMatrix();
        const bool needsNormal = material.shader().hasUniform("normalMatrix");
        // 求逆矩阵与非法变换校验只做一次，之后绘制使用同一结果。
        return {&mesh, &material, model,
            needsNormal ? lightingNormalMatrix(model) : glm::mat3(1.0f), needsNormal};
    }

    // 这一层只提交每个物体独有的uniform；Shader、材质和灯光状态由批次状态缓存处理。
    void drawPrepared(const PreparedDraw &item, const glm::mat4 &view,
        const glm::mat4 &projection, const glm::vec3 &cameraPosition)
    {
        const Shader &shader = item.material->shader();
        shader.setMat4("model", item.model);
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);
        if (item.material->pbrParameters())
        {
            shader.setVec3("cameraPosition", cameraPosition);
        }
        if (item.needsNormal) { shader.setMat3("normalMatrix", item.normal); }
        shader.setInt("skinEnabled", item.skin != nullptr && !item.skin->empty());
        if (item.skin && !item.skin->empty()) { shader.setMat4Array("jointMatrices[0]", *item.skin); }
        item.mesh->draw();
    }

    // 一次drawItems调用内的OpenGL提交缓存。资源对象本身不记录“当前是否绑定”，
    // 因为同一Material可能被不同Renderer或不同OpenGL上下文交替使用。
    class RenderSubmissionState
    {
    public:
        RenderSubmissionState(const LightingUniforms &lighting, const DirectionalShadowView *shadow)
            : lighting_(lighting), shadow_(shadow)
        {
        }

        void apply(const PreparedDraw &item)
        {
            const Shader &shader = item.material->shader();
            if (shader_ != &shader)
            {
                shader.use();
                shader.setInt("shadowEnabled", shadow_ != nullptr);
                if (shadow_ != nullptr)
                {
                    shader.setMat4("shadowMatrix", shadow_->lightMatrix);
                    shader.setFloat("shadowBias", shadow_->bias);
                    shader.setInt("shadowDepth", 5);
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, shadow_->texture);
                }
                // 光照数据在同一批次中不变；不同Shader仍必须分别上传，
                // 因为uniform location和链接结果属于各自的程序对象。
                lighting_.upload(shader);
                shader_ = &shader;
                material_ = nullptr;
                ++lightingUploads_;
            }
            if (material_ != item.material)
            {
                item.material->applyParameters();
                material_ = item.material;
                ++materialUploads_;
            }
        }

        std::size_t materialUploads() const noexcept { return materialUploads_; }
        std::size_t lightingUploads() const noexcept { return lightingUploads_; }

    private:
        const LightingUniforms &lighting_;
        const DirectionalShadowView *shadow_ = nullptr;
        const Shader *shader_ = nullptr;
        const Material *material_ = nullptr;
        std::size_t materialUploads_ = 0;
        std::size_t lightingUploads_ = 0;
    };

    // 作用域守卫：构造时保存，析构时恢复。后续draw抛异常也会自动执行恢复。
    // 这里只管理本次绘制会修改的状态，不接管帧缓冲、视口或调用者的uniform内容。
    class RenderState
    {
    public:
        RenderState()
        {
            framebufferSrgb_ = glIsEnabled(GL_FRAMEBUFFER_SRGB);
            depthTest_ = glIsEnabled(GL_DEPTH_TEST);
            blend_ = glIsEnabled(GL_BLEND);
            cull_ = glIsEnabled(GL_CULL_FACE);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite_);
            glGetIntegerv(GL_DEPTH_FUNC, &depthFunction_);
            glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRgb_);
            glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha_);
            glGetIntegerv(GL_BLEND_SRC_RGB, &sourceRgb_);
            glGetIntegerv(GL_BLEND_DST_RGB, &destinationRgb_);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &sourceAlpha_);
            glGetIntegerv(GL_BLEND_DST_ALPHA, &destinationAlpha_);
            glGetIntegerv(GL_CULL_FACE_MODE, &cullMode_);
            glGetIntegerv(GL_FRONT_FACE, &frontFace_);
            glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
            // 基础颜色0号、PBR贴图1..4号、阴影5号；恢复全部被触及的绑定。
            for (std::size_t i = 0; i < textures_.size(); ++i)
            {
                glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures_[i]);
            }
            glActiveTexture(activeTexture_);
        }

        ~RenderState()
        {
            restoreEnabled(GL_FRAMEBUFFER_SRGB, framebufferSrgb_);
            restoreEnabled(GL_DEPTH_TEST, depthTest_);
            restoreEnabled(GL_BLEND, blend_);
            restoreEnabled(GL_CULL_FACE, cull_);
            glDepthMask(depthWrite_);
            glDepthFunc(depthFunction_);
            glBlendEquationSeparate(equationRgb_, equationAlpha_);
            glBlendFuncSeparate(sourceRgb_, destinationRgb_, sourceAlpha_, destinationAlpha_);
            glCullFace(cullMode_);
            glFrontFace(frontFace_);
            glUseProgram(program_);
            glBindVertexArray(vao_);
            for (std::size_t i = 0; i < textures_.size(); ++i)
            {
                glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
                glBindTexture(GL_TEXTURE_2D, textures_[i]);
            }
            glActiveTexture(activeTexture_);
        }

        RenderState(const RenderState &) = delete;
        RenderState &operator=(const RenderState &) = delete;

        bool framebufferSrgb() const { return framebufferSrgb_ == GL_TRUE; }

    private:
        static void restoreEnabled(GLenum capability, GLboolean enabled)
        {
            if (enabled)
            {
                glEnable(capability);
            }
            else
            {
                glDisable(capability);
            }
        }

        GLboolean depthTest_ = GL_FALSE;
        GLboolean framebufferSrgb_ = GL_FALSE;
        GLboolean blend_ = GL_FALSE;
        GLboolean cull_ = GL_FALSE;
        GLboolean depthWrite_ = GL_TRUE;
        GLint depthFunction_ = 0;
        GLint equationRgb_ = 0;
        GLint equationAlpha_ = 0;
        GLint sourceRgb_ = 0;
        GLint destinationRgb_ = 0;
        GLint sourceAlpha_ = 0;
        GLint destinationAlpha_ = 0;
        GLint cullMode_ = 0;
        GLint frontFace_ = 0;
        GLint program_ = 0;
        GLint vao_ = 0;
        GLint activeTexture_ = 0;
        std::array<GLint, 6> textures_{};
    };
}

void Renderer::drawItems(const std::vector<RenderItem> &items, const Camera &camera, float aspectRatio) const
{
    static const DirectionalLight defaultLight{};
    drawItems(items, camera, aspectRatio, defaultLight);
}

void Renderer::drawItems(const std::vector<RenderItem> &items, const Camera &camera,
    float aspectRatio, const DirectionalLight &light) const
{
    drawItems(items, camera, aspectRatio, SceneLighting::fromDirectionalLight(light));
}

void Renderer::drawItems(const std::vector<RenderItem> &items, const Camera &camera,
    float aspectRatio, const SceneLighting &lighting, const DirectionalShadowView *shadow,
    bool requireLinearOutput) const
{
    lastStats_ = {};
    const LightingUniforms prepared(lighting);
    if (shadow)
    {
        if (!shadow->texture || !glIsTexture(shadow->texture) || !std::isfinite(shadow->bias) || shadow->bias < 0)
        { throw std::invalid_argument("Invalid or expired shadow view"); }
        for (int c=0;c<4;++c) { for (int r=0;r<4;++r)
        { if (!std::isfinite(shadow->lightMatrix[c][r])) { throw std::invalid_argument("Invalid shadow matrix"); } } }
    }
    if (items.empty())
    {
        return;
    }
    const auto view = camera.viewMatrix();
    const auto projection = camera.projectionMatrix(aspectRatio);
    std::vector<PreparedDraw> opaque;
    std::vector<PreparedDraw> transparent;
    opaque.reserve(items.size());
    transparent.reserve(items.size());
    for (const auto &item : items)
    {
        if (item.mesh == nullptr || item.material == nullptr || item.transform == nullptr)
        {
            throw std::invalid_argument("RenderItem requires mesh, material and transform");
        }
        prepared.validateShader(item.material->shader());
        if (requireLinearOutput && item.material->shaderOutputsSrgb())
        { throw std::invalid_argument("HDR scene cannot contain a shader that already outputs sRGB"); }
        if (shadow && !item.material->shader().hasUniform("shadowEnabled"))
        { throw std::invalid_argument("Directional shadows require a shadow-aware shader (builtin PBR)"); }
        // 在绘制任何物体之前检查需要光照的变换，错误不会留下半帧内容。
        auto draw = prepareDraw(*item.mesh, *item.material, *item.transform);
        if (!item.skinMatrices.empty())
        {
            const auto &shader = item.material->shader();
            if (!item.mesh->hasSkinAttributes() || item.mesh->maximumJoint() >= item.skinMatrices.size() ||
                !shader.hasUniform("skinEnabled") || item.skinMatrices.size() > shader.matrixArrayCapacity("jointMatrices[0]"))
            { throw std::invalid_argument("Skin palette does not match the mesh or shader capacity"); }
            for (const auto &matrix : item.skinMatrices)
            {
                for (int column = 0; column < 4; ++column)
                {
                    for (int row = 0; row < 4; ++row)
                    { if (!std::isfinite(matrix[column][row])) { throw std::invalid_argument("Non-finite skin palette"); } }
                }
            }
            draw.skin = &item.skinMatrices;
        }
        if (item.material->renderMode() == RenderMode::AlphaBlend)
        {
            draw.depth = transparentViewDepth(draw.model, item.sortOrigin, view);
            transparent.push_back(draw);
        }
        else
        {
            opaque.push_back(draw);
        }
    }
    // 不透明物体依赖深度缓冲解决遮挡；透明物体不能依赖深度写入，
    // 所以必须在不透明组完成后单独按“远到近”绘制。
    // 所有校验和排序在修改OpenGL状态之前完成；不改变Scene的存储顺序。
    std::stable_sort(transparent.begin(), transparent.end(),
        [](const PreparedDraw &left, const PreparedDraw &right) { return left.depth < right.depth; });
    // 不透明物体不依赖绘制顺序解决可见性，可以按GPU状态分组，减少Shader和材质切换。
    // 使用指针地址作为稳定的资源标识；std::less保证不同对象指针比较满足严格弱序。
    std::stable_sort(opaque.begin(), opaque.end(), [](const PreparedDraw &left, const PreparedDraw &right)
    {
        const std::less<const Shader *> lessShader;
        if (&left.material->shader() != &right.material->shader())
        {
            return lessShader(&left.material->shader(), &right.material->shader());
        }
        if (left.material != right.material)
        {
            return std::less<const Material *>{}(left.material, right.material);
        }
        return std::less<const Mesh *>{}(left.mesh, right.mesh);
    });
    lastStats_.opaqueItems = opaque.size();
    lastStats_.transparentItems = transparent.size();

    const Shader *previousShader = nullptr;
    const Material *previousMaterial = nullptr;
    const Mesh *previousMesh = nullptr;
    const RenderState savedState;
    RenderSubmissionState submission(prepared, shadow);
    setDepthTestEnabled(true);
    setDepthWriteEnabled(true);
    setAlphaBlendingEnabled(false);
    const auto drawItem = [&](const PreparedDraw &item)
    {
        const Shader *shader = &item.material->shader();
        if (shader != previousShader) { ++lastStats_.shaderChanges; }
        if (item.material != previousMaterial) { ++lastStats_.materialChanges; }
        if (item.mesh != previousMesh) { ++lastStats_.meshChanges; }
        previousShader = shader;
        previousMaterial = item.material;
        previousMesh = item.mesh;
        ++lastStats_.drawCalls;
        setFaceCullingEnabled(item.material->cullMode() == CullMode::Back);
        if (item.material->correctMirroredWinding())
        {
            // 只看自身scale不够：父节点的负缩放也会改变最终三角形绕序。
            const auto orientation = glm::determinant(glm::mat3(item.model));
            glFrontFace(orientation < 0 ? GL_CW : GL_CCW);
        }
        if (item.material->shaderOutputsSrgb() || !savedState.framebufferSrgb()) { glDisable(GL_FRAMEBUFFER_SRGB); }
        else { glEnable(GL_FRAMEBUFFER_SRGB); }
        submission.apply(item);
        drawPrepared(item, view, projection, camera.position());
    };
    for (const auto &item : opaque)
    {
        drawItem(item);
    }
    if (!transparent.empty())
    {
        // 透明表面需要前面的深度遮挡，但不能把自己的深度写入后挡住其他透明层。
        setAlphaBlendingEnabled(true);
        setDepthWriteEnabled(false);
        for (const auto &item : transparent)
        {
            drawItem(item);
        }
    }
    lastStats_.materialUploads = submission.materialUploads();
    lastStats_.lightingUploads = submission.lightingUploads();
    // savedState离开作用域时恢复调用者状态，而不是强行恢复为某套默认值。
}

void Renderer::clear(float red, float green, float blue, float alpha) const
{
    // 深度写入关闭时，OpenGL也会跳过深度缓冲清理。先保存状态并临时允许写入。
    GLboolean previousDepthWrite = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthWrite);
    glDepthMask(GL_TRUE);

    // 设置清屏颜色
    glClearColor(red, green, blue, alpha);
    // 设置清屏时的深度初始值，并同时清空颜色和深度缓冲区。
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // clear不应改变调用者为后续绘制设置的深度写入状态。
    glDepthMask(previousDepthWrite);
}

void Renderer::setDepthTestEnabled(bool enabled) const
{
    if (enabled)
    {
        // 深度值更小的片段更靠近摄像机，可以覆盖更远的片段。
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
}

void Renderer::setDepthWriteEnabled(bool enabled) const
{
    // 深度测试控制片段能否通过；深度写入独立控制通过的片段是否更新深度缓冲。
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void Renderer::setAlphaBlendingEnabled(bool enabled) const
{
    if (enabled)
    {
        // 项目中的PNG等图片使用普通RGBA（非预乘Alpha），颜色通道按源Alpha混合。
        // Alpha通道单独按覆盖关系累积，避免源Alpha被重复相乘。
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(
            GL_SRC_ALPHA,
            GL_ONE_MINUS_SRC_ALPHA,
            GL_ONE,
            GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
    }
    else
    {
        // 关闭混合后，片段颜色将直接覆盖目标颜色。
        glDisable(GL_BLEND);
    }
}

void Renderer::setFaceCullingEnabled(bool enabled) const
{
    if (enabled)
    {
        // 背面剔除根据屏幕上的顶点绕序判断，不读取顶点颜色、UV或法线属性。
        // 每次启用都明确重设约定，避免外部代码留下GL_FRONT或GL_CW等状态。
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_CULL_FACE);
    }
    else
    {
        // 只停止剔除，后续绘制可显示两面；深度测试仍按原有设置工作。
        glDisable(GL_CULL_FACE);
    }
}

void Renderer::draw(
    const Mesh &mesh,
    const Material &material,
    const Transform &transform,
    const Camera &camera,
    float aspectRatio) const
{
    static const DirectionalLight defaultLight{};
    draw(mesh, material, transform, camera, aspectRatio, defaultLight);
}

void Renderer::draw(
    const Mesh &mesh,
    const Material &material,
    const Transform &transform,
    const Camera &camera,
    float aspectRatio,
    const DirectionalLight &light) const
{
    draw(mesh, material, transform, camera, aspectRatio, SceneLighting::fromDirectionalLight(light));
}

void Renderer::draw(const Mesh &mesh, const Material &material, const Transform &transform,
    const Camera &camera, float aspectRatio, const SceneLighting &lighting) const
{
    const LightingUniforms prepared(lighting);
    prepared.validateShader(material.shader());
    const auto item = prepareDraw(mesh, material, transform);
    RenderSubmissionState submission(prepared, nullptr);
    submission.apply(item);
    drawPrepared(item, camera.viewMatrix(), camera.projectionMatrix(aspectRatio), camera.position());
}
