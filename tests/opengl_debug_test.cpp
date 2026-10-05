#include "TestSupport.h"
#include "core/Log.h"
#include "graphics/rendering/OpenGLDebug.h"
#include "graphics/resources/Shader.h"
#include "platform/Window.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace
{
    std::string readFile(const std::filesystem::path &path)
    {
        std::ifstream input(path);
        require(input.good(), "Cannot read diagnostic log");
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    // 错误Shader是测试专属资源，不混入引擎或可运行示例。
    void expectShaderFailure(const std::string &vertex, const std::string &fragment,
        const std::string &prefix, const std::filesystem::path &logPath)
    {
        const std::string previousLog = readFile(logPath);
        std::string detail;
        try
        {
            Shader invalid(vertex, fragment);
        }
        catch (const std::runtime_error &exception)
        {
            detail = exception.what();
        }
        require(!detail.empty(), "Expected shader failure did not occur");
        require(detail.find(prefix) == 0 && detail.find(vertex) != std::string::npos,
            "Shader error omitted stage or source path");
        const auto separator = detail.find(":\n");
        require(separator != std::string::npos && separator + 2 < detail.size(),
            "Shader error omitted driver diagnostic");
        if (prefix.find("linking") != std::string::npos)
        {
            require(detail.find(fragment) != std::string::npos, "Link error omitted fragment path");
        }
        // 框架只携带错误信息，应用捕获后记录一次，不应出现提前输出。
        require(readFile(logPath) == previousLog, "Shader logged exception twice");
        LOG_ERROR(detail);
        require(readFile(logPath).find(detail) != std::string::npos, "Shader diagnostic was lost in logging");
    }
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected test log path");
        const std::filesystem::path logPath(argv[1]);
        std::filesystem::remove(logPath);
        Log::setConsoleEnabled(false);
        require(Log::setFile(logPath), "Cannot create diagnostic log");
        require(!OpenGLDebug::checkErrors("before context", __FILE__, __LINE__),
            "Missing context reported success");
        require(readFile(logPath).find("no current OpenGL context") != std::string::npos,
            "Missing context was not explained");

        {
            Window window(64, 64, "OpenGL Diagnostic Test", false);
            require(OpenGLDebug::checkErrors("initialization", __FILE__, __LINE__),
                "Unexpected initialization error");
            const auto cleanLog = readFile(logPath);
            require(OpenGLDebug::checkErrors("clean context", __FILE__, __LINE__) &&
                readFile(logPath) == cleanLog, "Clean context produced error output");

            // 使用真实驱动制造三种错误；每次立刻检查，避免驱动合并多个错误标记。
            glEnable(0xFFFFFFFFu);
            const int errorLine = __LINE__ + 1;
            require(!OpenGLDebug::checkErrors("invalid capability", __FILE__, errorLine), "Invalid enum missed");
            require(glGetError() == GL_NO_ERROR, "Diagnostic did not consume pending error");
            glViewport(0, 0, -1, 64);
            require(!OpenGLDebug::checkErrors("negative viewport", __FILE__, __LINE__), "Invalid value missed");
            glUseProgram(0);
            glUniform1f(0, 1.0f);
            require(!OpenGLDebug::checkErrors("uniform without program", __FILE__, __LINE__),
                "Invalid operation missed");
            const auto errors = readFile(logPath);
            require(errors.find("invalid capability: GL_INVALID_ENUM (0x500)") != std::string::npos &&
                errors.find("GL_INVALID_VALUE (0x501)") != std::string::npos &&
                errors.find("GL_INVALID_OPERATION (0x502)") != std::string::npos &&
                errors.find("opengl_debug_test.cpp:" + std::to_string(errorLine)) != std::string::npos,
                "Diagnostic omitted error name, code, operation or call site");

            // 日志被关闭时仍需要返回失败并消费错误，不得让日志配置改变判断结果。
            Log::setLevel(Log::Level::Off);
            glEnable(0xFFFFFFFFu);
            require(!OpenGLDebug::checkErrors("filtered error", __FILE__, __LINE__) &&
                glGetError() == GL_NO_ERROR, "Filtered error returned success or remained pending");
            require(readFile(logPath) == errors, "Off level still logged OpenGL error");
            Log::setLevel(Log::Level::Info);

            // 同一测试会分别以普通和NDEBUG方式编译，验证宏是否按约定被移除。
            int evaluations = 0;
            glEnable(0xFFFFFFFFu);
            INFINITE_GL_CHECK(std::to_string(++evaluations));
#ifdef NDEBUG
            require(evaluations == 0 && glGetError() == GL_INVALID_ENUM,
                "Release macro evaluated expression or consumed driver error");
#else
            require(evaluations == 1 && glGetError() == GL_NO_ERROR,
                "Debug macro skipped evaluation or driver check");
#endif

            expectShaderFailure("tests/fixtures/shaders/invalid.vert", "tests/fixtures/shaders/link.frag",
                "Shader compilation failed", logPath);
            expectShaderFailure("tests/fixtures/shaders/link.vert", "tests/fixtures/shaders/link.frag",
                "Shader program linking failed", logPath);
            // 编译/链接失败走独立状态查询，不能依赖glGetError；失败后仍可创建正常Shader。
            Shader valid("examples/alpha_blending/shaders/color.vert", "examples/alpha_blending/shaders/color.frag");
            valid.use();
            require(OpenGLDebug::checkErrors("after shader failures", __FILE__, __LINE__),
                "Shader failure left invalid OpenGL state");
        }
        require(!OpenGLDebug::checkErrors("after context", __FILE__, __LINE__),
            "Destroyed context reported success");
        Log::closeFile();
        std::cout << "OpenGL diagnostics passed: context, real driver errors, build mode and shader failures" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "OpenGL diagnostic test failed: " << exception.what() << std::endl;
        return 1;
    }
}
