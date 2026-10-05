#include "PbrResources.h"
#include "graphics/resources/Shader.h"
#include "graphics/resources/Material.h"
#include "PbrShaderSources.h"
#include "graphics/shaders/SkinShaderSource.h"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <algorithm>
#include <string>

std::shared_ptr<Shader> PbrResources::shader()
{
    if (!glfwGetCurrentContext()) { throw std::logic_error("PbrResources requires a current OpenGL context"); }
    if (!shader_)
    {
        shader_ = std::make_shared<Shader>(Shader::fromSource(configureSkinShader(PbrShaders::vertex),
            PbrShaders::fragment, "builtin PBR"));
    }
    return shader_;
}

std::shared_ptr<Material> PbrResources::createMaterial(const glm::vec4 &color, const PbrParameters &parameters)
{
    parameters.validate();
    auto result = std::make_shared<Material>(shader(), color);
    result->setPbrParameters(parameters);
    result->setCorrectMirroredWinding(true);
    result->setCullMode(CullMode::Back);
    return result;
}
