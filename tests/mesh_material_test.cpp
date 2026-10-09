#include "TestSupport.h"
#include "graphics/camera/Camera.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Texture.h"
#include "platform/Window.h"
#include "scene/Scene.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>

namespace
{
    GLint state(GLenum name)
    {
        GLint value = 0;
        glGetIntegerv(name, &value);
        return value;
    }

    std::array<unsigned char, 4> pixel()
    {
        std::array<unsigned char, 4> result{};
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
        return result;
    }
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected test output directory");
        Window window(128, 128, "Mesh and Material Test", false);
        glViewport(0, 0, 64, 64);
        glDisable(GL_DITHER);
        // Unicode路径通过filesystem::path传递，不转换成系统窄字符串。
        const auto directory = std::filesystem::path(argv[1]) / std::filesystem::u8path(u8"着色器测试");
        std::filesystem::create_directories(directory);
        std::filesystem::copy_file("examples/scene_objects/shaders/scene.vert", directory / "scene.vert", std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file("examples/scene_objects/shaders/scene.frag", directory / "scene.frag", std::filesystem::copy_options::overwrite_existing);
        Shader shader(directory / "scene.vert", directory / "scene.frag");

        const std::vector<Vertex> vertices = {
            {{-0.8f, -0.8f, 0}, {1, 1, 1}, {0, 0}},
            {{ 0.8f, -0.8f, 0}, {1, 1, 1}, {1, 0}},
            {{ 0.8f,  0.8f, 0}, {1, 1, 1}, {1, 1}},
            {{-0.8f,  0.8f, 0}, {1, 1, 1}, {0, 1}}
        };
        const std::vector<std::uint32_t> indices{0, 1, 2, 2, 3, 0};
        GLuint callerVao = 0, callerBuffer = 0;
        glGenVertexArrays(1, &callerVao);
        glGenBuffers(1, &callerBuffer);
        glBindVertexArray(callerVao);
        glBindBuffer(GL_ARRAY_BUFFER, callerBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, callerBuffer);
        Mesh indexed(vertices, indices);
        require(indexed.vertexCount() == 4 && indexed.indexCount() == 6, "Counts incorrect");
        require(indexed.bounds().minimum == vertices[0].position && indexed.bounds().maximum == vertices[2].position, "Bounds incorrect");
        const auto bindingsCorrect = [&]
        {
            return state(GL_VERTEX_ARRAY_BINDING) == static_cast<GLint>(callerVao) &&
                state(GL_ARRAY_BUFFER_BINDING) == static_cast<GLint>(callerBuffer) &&
                state(GL_ELEMENT_ARRAY_BUFFER_BINDING) == static_cast<GLint>(callerBuffer);
        };
        require(bindingsCorrect(), "Mesh creation leaked bindings");
        expectThrow<std::invalid_argument>([&] { Mesh invalid(vertices, {0, 1, 4}); }, "Out-of-range index accepted");
        expectThrow<std::invalid_argument>([&] { Mesh invalid(vertices, {0, 1}); }, "Incomplete triangle accepted");
        expectThrow<std::invalid_argument>([] { Mesh invalid(std::vector<float>{1}); }, "Incomplete layout accepted");
        expectThrow<std::invalid_argument>([] { Mesh invalid(std::vector<Vertex>{}); }, "Empty mesh accepted");
        auto invalidVertices = vertices;
        invalidVertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
        expectThrow<std::invalid_argument>([&] { Mesh invalid(invalidVertices, indices); }, "NaN vertex accepted");
        require(bindingsCorrect(), "Invalid creation changed bindings");

        std::vector<Vertex> expanded;
        for (auto index : indices) { expanded.push_back(vertices[index]); }
        Mesh arrays(expanded);
        Mesh moved(std::move(indexed));
        Mesh assigned(expanded);
        assigned = std::move(moved);
        require(assigned.indexCount() == 6 && moved.vertexCount() == 0, "Move lost EBO");
        expectThrow<std::logic_error>([&] { moved.draw(); }, "Moved-from Mesh drawn");
        Material material(shader, {1, 0, 0, 1});
        Camera camera;
        Renderer renderer;
        Transform transform;
        const auto draw = [&](const Mesh &mesh)
        {
            renderer.clear(0, 0, 0, 1);
            renderer.drawItems({{&mesh, &material, &transform}}, camera, 1);
            require(bindingsCorrect(), "Drawing leaked bindings");
            std::vector<unsigned char> pixels(64 * 64 * 4);
            glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            return pixels;
        };
        require(draw(arrays) == draw(assigned), "Indexed rendering differs from expanded rendering");
        require(pixel()[0] > 250 && pixel()[1] < 2, "No-texture material did not render red");
        ImageData image;
        image.width = image.height = 1;
        image.pixels = {0, 255, 0, 255};
        Texture green(image);
        material.setBaseColor(glm::vec4(1));
        material.setTexture(&green);
        draw(assigned);
        require(pixel()[1] > 250 && pixel()[0] < 2, "Texture setter not applied");
        material.setTexture(nullptr);
        material.setBaseColor({1, 0, 0, 1});
        draw(assigned);
        require(pixel()[0] > 250 && pixel()[1] < 2, "Removed texture leaked previous material");
        require(material.texture() == nullptr && material.baseColor().r == 1, "Material getters wrong");
        expectThrow<std::invalid_argument>([&] { material.setBaseColor({1, 1, 1, -1}); }, "Invalid alpha accepted");

        // 材质实例只复制表面参数，底层Shader/Texture仍可共享；修改副本不能污染原材质。
        auto materialInstance = material.clone();
        materialInstance->setBaseColor({0, 0, 1, 1});
        materialInstance->setRenderMode(RenderMode::AlphaBlend);
        require(material.baseColor().r == 1 && material.baseColor().b == 0 &&
                material.renderMode() == RenderMode::Opaque,
            "Material clone changed the shared source");
        require(&materialInstance->shader() == &material.shader(),
            "Material clone did not share the Shader resource");

        Scene scene;
        std::weak_ptr<Mesh> meshLifetime;
        std::weak_ptr<Material> materialLifetime;
        std::weak_ptr<Shader> shaderLifetime;
        std::weak_ptr<Texture> textureLifetime;
        auto &object = scene.createObject("Owned resources");
        require(!object.renderable().isBound() && object.renderable().mesh() == nullptr &&
            object.renderable().material() == nullptr && object.mesh() == nullptr &&
            object.material() == nullptr, "Renderable component was not initially empty");
        {
            auto sharedShader = std::make_shared<Shader>(directory / "scene.vert", directory / "scene.frag");
            auto sharedTexture = std::make_shared<Texture>(image);
            auto sharedMesh = std::make_shared<Mesh>(vertices, indices);
            auto sharedMaterial = std::make_shared<Material>(sharedShader, glm::vec4(1), sharedTexture);
            meshLifetime = sharedMesh;
            materialLifetime = sharedMaterial;
            shaderLifetime = sharedShader;
            textureLifetime = sharedTexture;
            object.setRenderable(sharedMesh, sharedMaterial);
            require(object.renderable().isBound() && object.renderable().mesh() == sharedMesh.get() &&
                object.renderable().material() == sharedMaterial.get() &&
                object.mesh() == object.renderable().mesh() &&
                object.material() == object.renderable().material(),
                "GameObject renderable compatibility accessors disagree");
            object.renderable().setSortOrigin({1.0f, 2.0f, 3.0f});
            require(object.sortOrigin() == object.renderable().sortOrigin() &&
                object.sortOrigin().z == 3.0f, "Renderable sort origin was not exposed by GameObject");
            // 重复用原指针设置时，不应丢掉已经建立的持有关系。
            object.setRenderable(*sharedMesh, *sharedMaterial);
            sharedMaterial->setTexture(sharedTexture.get());
            expectThrow<std::invalid_argument>([&] { object.setRenderable(std::shared_ptr<Mesh>{}, sharedMaterial); }, "Empty owned resource accepted");
        }
        require(!meshLifetime.expired() && !shaderLifetime.expired(), "Resources expired with caller scope");
        renderer.clear(0, 0, 0, 1);
        scene.render(renderer, camera, 1);
        require(pixel()[1] > 250, "Scene failed after caller released shared resources");
        object.clearRenderable();
        require(!object.renderable().isBound() && object.mesh() == nullptr && object.material() == nullptr,
            "Clearing GameObject did not clear Renderable component");
        require(meshLifetime.expired() && materialLifetime.expired() && shaderLifetime.expired() && textureLifetime.expired(), "Unbind leaked owned resources");
        scene.clear();
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteVertexArrays(1, &callerVao);
        glDeleteBuffers(1, &callerBuffer);
        require(glGetError() == GL_NO_ERROR, "OpenGL error in Mesh/Material test");
        std::cout << "Mesh, material, Unicode paths and shared lifetimes passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
