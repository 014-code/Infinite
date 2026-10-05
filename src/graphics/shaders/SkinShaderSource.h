#pragma once
#include <string>

// 为包含容量标记的顶点Shader填入本机可用关节数；调用时需要当前OpenGL上下文。
// PBR和阴影共用规则，防止颜色通道能蒙皮但深度通道数组不够。
std::string configureSkinShader(const std::string &source);
