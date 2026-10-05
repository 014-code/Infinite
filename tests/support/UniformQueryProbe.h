#pragma once

#include <GL/glew.h>
#include <cstddef>
#include <stdexcept>

// 仅用于单线程测试：临时包装GLEW的查询入口，统计真实驱动查询，不给引擎公开接口增加计数器。
// 必须在Window初始化GLEW后创建；作用域结束自动恢复函数指针，不能嵌套或跨线程使用。
class UniformQueryProbe final
{
public:
    UniformQueryProbe()
    {
        if (original_ != nullptr) { throw std::logic_error("UniformQueryProbe cannot be nested"); }
        original_ = __glewGetUniformLocation;
        calls_ = 0;
        __glewGetUniformLocation = query;
    }
    ~UniformQueryProbe()
    {
        __glewGetUniformLocation = original_;
        original_ = nullptr;
    }
    UniformQueryProbe(const UniformQueryProbe &) = delete;
    UniformQueryProbe &operator=(const UniformQueryProbe &) = delete;
    std::size_t calls() const { return calls_; }
    void reset() { calls_ = 0; }

private:
    static GLint GLAPIENTRY query(GLuint program, const GLchar *name)
    {
        ++calls_;
        return original_(program, name);
    }
    inline static PFNGLGETUNIFORMLOCATIONPROC original_ = nullptr;
    inline static std::size_t calls_ = 0;
};
