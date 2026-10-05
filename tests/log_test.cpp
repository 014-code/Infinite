#include "TestSupport.h"
#include "core/Log.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
    std::string readFile(const std::filesystem::path &path)
    {
        std::ifstream input(path);
        require(input.good(), "Cannot read test log");
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    // 临时捕获控制台，离开作用域时恢复；测试失败抛异常也不会留下悬空的流指针。
    class ConsoleCapture
    {
    public:
        ConsoleCapture() : previous_(std::cerr.rdbuf(output_.rdbuf())) {}
        ~ConsoleCapture() { std::cerr.rdbuf(previous_); }
        std::string text() const { return output_.str(); }

        ConsoleCapture(const ConsoleCapture &) = delete;
        ConsoleCapture &operator=(const ConsoleCapture &) = delete;

    private:
        std::ostringstream output_;
        std::streambuf *previous_;
    };
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected test output directory");
        const auto directory = std::filesystem::path(argv[1]);
        const auto path = directory / "nested/log_test.log";
        // 只清理本测试的已知文件，使重复运行不受上一次结果影响。
        std::filesystem::remove(path);
        ConsoleCapture console;

        LOG_INFO("console before configuration");
        require(console.text().find("console before configuration") != std::string::npos,
            "Default console logging is unavailable");
        require(Log::setFile(path), "Cannot create log file and parent directories");

        int evaluations = 0;
        LOG_DEBUG(std::to_string(++evaluations));
        require(evaluations == 0, "Filtered message was evaluated");
        const int sourceLine = __LINE__ + 1;
        LOG_INFO("中文日志");
        // 不关闭日志就读取，验证每次写入会刷新，崩溃前的信息不会只留在C++缓冲区。
        std::string contents = readFile(path);
        require(contents.find("[INFO] [log_test.cpp:" + std::to_string(sourceLine) + "] 中文日志")
            != std::string::npos, "Missing level, Chinese message or caller source location");
        require(std::regex_search(contents,
            std::regex(R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\])")), "Missing timestamp");
        require(console.text().find(contents) != std::string::npos, "Console and file differ");

        Log::setLevel(Log::Level::Warning);
        LOG_INFO("filtered info");
        LOG_WARN("visible warning");
        LOG_ERROR("visible error");
        Log::setLevel(Log::Level::Off);
        LOG_ERROR(std::to_string(++evaluations));
        Log::write(Log::Level::Off, "off sentinel", __FILE__, __LINE__);
        require(evaluations == 0, "Off level evaluated message");
        Log::setLevel(Log::Level::Debug);
        LOG_DEBUG("visible debug");
        contents = readFile(path);
        require(contents.find("filtered info") == std::string::npos &&
            contents.find("off sentinel") == std::string::npos &&
            contents.find("[WARN]") != std::string::npos &&
            contents.find("[ERROR]") != std::string::npos &&
            contents.find("[DEBUG]") != std::string::npos, "Level filtering failed");

        // 控制台开关不应影响文件记录。
        Log::setConsoleEnabled(false);
        LOG_INFO("file only");
        require(console.text().find("file only") == std::string::npos &&
            readFile(path).find("file only") != std::string::npos, "Console toggle affected file output");

        // 多个线程共用一个日志入口，完整记录不能互相穿插或丢失。
        std::vector<std::thread> workers;
        for (int worker = 0; worker < 4; ++worker)
        {
            workers.emplace_back([worker]()
            {
                for (int record = 0; record < 20; ++record)
                {
                    LOG_INFO("worker-" + std::to_string(worker) + "-" + std::to_string(record) + "-end");
                }
            });
        }
        for (auto &worker : workers)
        {
            worker.join();
        }
        contents = readFile(path);
        for (int worker = 0; worker < 4; ++worker)
        {
            for (int record = 0; record < 20; ++record)
            {
                const std::string marker = "] worker-" + std::to_string(worker)
                    + "-" + std::to_string(record) + "-end\n";
                const auto position = contents.find(marker);
                require(position != std::string::npos &&
                    contents.find(marker, position + 1) == std::string::npos,
                    "Concurrent log record missing, duplicated or interleaved");
            }
        }

        Log::closeFile();
        const std::string beforeReopen = readFile(path);
        require(Log::setFile(path), "Cannot reopen log");
        LOG_INFO("appended record");
        require(readFile(path).find(beforeReopen) == 0, "Reopening truncated old records");

        // 把普通文件当作父目录，稳定触发打开失败，不依赖本机目录权限。
        require(!Log::setFile(path / "impossible.log"), "Invalid file path was accepted");
        LOG_ERROR("old file remains active");
        require(readFile(path).find("old file remains active") != std::string::npos,
            "Failed file switch lost original output");
        Log::closeFile();
        const std::string afterClose = readFile(path);
        Log::setConsoleEnabled(true);
        LOG_INFO("console after close");
        require(readFile(path) == afterClose &&
            console.text().find("console after close") != std::string::npos, "Closing file broke console");
        std::cout << "Log tests passed: levels, location, UTF-8, flush, threads, append and failure fallback" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Log test failed: " << exception.what() << std::endl;
        return 1;
    }
}
