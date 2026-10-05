#pragma once

#include <string_view>

namespace OpenGLDebug
{
    // 检查当前线程上下文中的待处理错误，全部记录后返回是否无错误。
    // glGetError会消费错误状态；不要再指望之后的glGetError重复读到同一错误。
    // operation描述检查区间，不能保证错误一定来自紧邻的上一条OpenGL调用。
    // 没有当前上下文时记录错误并返回false，不调用glGetError。
    bool checkErrors(std::string_view operation, const char *file, int line);
}

// 自动检查只用于非NDEBUG构建。Release不查询驱动，也不会计算operation表达式。
// 测试或排障时可以直接调用checkErrors，它在所有构建类型中都可用。
#ifndef NDEBUG
#define INFINITE_GL_CHECK(operation) \
    do \
    { \
        ::OpenGLDebug::checkErrors((operation), __FILE__, __LINE__); \
    } while (false)
#else
#define INFINITE_GL_CHECK(operation) ((void)0)
#endif
