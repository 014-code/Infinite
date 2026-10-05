#pragma once

#include <stdexcept>
#include <string>

// 新增测试共用的最小断言，不引入额外测试框架，也不依赖Debug构建的assert。
inline void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

template<class Exception, class Function>
void expectThrow(Function action, const char *message)
{
    bool rejected = false;
    try { action(); }
    catch (const Exception &) { rejected = true; }
    require(rejected, message);
}
