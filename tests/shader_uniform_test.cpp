#include "TestSupport.h"
#include "support/UniformQueryProbe.h"
#include "graphics/resources/Shader.h"
#include "platform/Window.h"

#include <cmath>
#include <iostream>
#include <utility>

namespace
{
    const std::string vertex = R"(
        #version 330 core
        layout(location = 0) in vec3 position;
        uniform mat4 matrix;
        uniform mat3 basis;
        uniform vec3 offset;
        uniform float weight;
        uniform int mode;
        void main() { gl_Position = matrix * vec4(basis * position + offset * weight, 1.0 + float(mode)); }
    )";
    const std::string fragment = R"(
        #version 330 core
        uniform vec4 tint;
        out vec4 color;
        void main() { color = tint; }
    )";
}

int main()
{
    try
    {
        Window window(64, 64, "Shader uniform cache test", false);
        auto shader = Shader::fromSource(vertex, fragment);
        const auto directLookup = glGetUniformLocation; // 读回验证绕过计数探针，避免污染被测次数。
        UniformQueryProbe probe;
        shader.use();
        for (int repeat = 0; repeat < 10; ++repeat)
        {
            require(shader.hasUniform("matrix"), "Active matrix missing");
            shader.setMat4("matrix", glm::mat4(2));
            shader.setMat3("basis", glm::mat3(3));
            shader.setVec3("offset", {4, 5, 6});
            shader.setFloat("weight", .5f);
            shader.setInt("mode", 2);
            shader.setVec4("tint", {.2f, .3f, .4f, 1});
            require(!shader.hasUniform("missing"), "Missing uniform reported active");
            // 所有setter也复用-1缓存，不能产生GL错误或逐次重新查询。
            shader.setMat4("missing", glm::mat4(1));
            shader.setMat3("missing", glm::mat3(1));
            shader.setVec3("missing", glm::vec3(1));
            shader.setVec4("missing", glm::vec4(1));
            shader.setFloat("missing", 1);
            shader.setInt("missing", 1);
        }
        require(probe.calls() == 7, "Uniform names were queried more than once (including -1)");
        GLint program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        const auto expectValue = [&](const char *name, float expected)
        {
            GLfloat values[16]{};
            glGetUniformfv(program, directLookup(program, name), values);
            require(std::abs(values[0] - expected) < .0001f, std::string("Wrong upload for ") + name);
        };
        expectValue("matrix", 2); expectValue("basis", 3); expectValue("offset", 4);
        expectValue("weight", .5f); expectValue("tint", .2f);
        GLint mode = 0;
        glGetUniformiv(program, directLookup(program, "mode"), &mode);
        require(mode == 2, "Cached integer upload failed");

        probe.reset();
        auto moved = std::move(shader);
        require(moved.hasUniform("matrix") && !moved.hasUniform("missing"), "Move construction lost cache");
        require(!shader.hasUniform("matrix"), "Moved-from Shader kept active uniforms");
        require(probe.calls() == 0, "Move construction repeated a cached query");
        // 目标原来的程序没有tint，先缓存其-1，再移动赋值，验证旧程序的缓存被完整替换。
        auto destination = Shader::fromSource(vertex, "#version 330 core\nout vec4 color; void main(){color=vec4(1);}");
        require(!destination.hasUniform("tint"), "Destination should not have tint before move");
        probe.reset();
        destination = std::move(moved);
        require(destination.hasUniform("tint"), "Move assignment reused old program's -1 cache");
        destination.use();
        destination.setVec4("tint", {.7f, 0, 0, 1});
        expectValue("tint", .7f);
        require(!moved.hasUniform("tint") && probe.calls() == 0, "Move assignment did not transfer cache");

        // 新链接的同名uniform必须独立查询，不能用一个全局name->location缓存。
        auto independent = Shader::fromSource(vertex, fragment);
        require(independent.hasUniform("tint") && probe.calls() == 1, "Independent program reused another cache");
        require(glGetError() == GL_NO_ERROR, "Uniform cache test produced OpenGL error");
        std::cout << "Shader uniform cache passed: setters, missing names, move and program isolation\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Shader uniform cache failed: " << error.what() << '\n';
        return 1;
    }
}
