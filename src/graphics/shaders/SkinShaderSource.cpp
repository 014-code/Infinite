#include "SkinShaderSource.h"
#include <GL/glew.h>
#include <algorithm>
#include <stdexcept>

std::string configureSkinShader(const std::string &source)
{
    GLint components = 0;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &components);
    // 每个mat4占16分量，预留128给model/view/projection/normal及其他顶点uniform。
    const int joints = std::min(64, (components - 128) / 16);
    if (joints < 1) { throw std::runtime_error("Insufficient vertex uniform capacity"); }
    std::string result = source;
    const std::string marker = "// @skin_capacity@";
    const auto offset = result.find(marker);
    if (offset == std::string::npos) { throw std::logic_error("Missing skin capacity shader marker"); }
    result.replace(offset, marker.size(), "#define MAX_SKIN_JOINTS " + std::to_string(joints));
    return result;
}
