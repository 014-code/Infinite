#include "PendingSceneFile.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace scene_serialization
{
    namespace
    {
        std::runtime_error fileError(const char *operation, const std::filesystem::path &path, int error)
        {
            return std::runtime_error(std::string(operation) + ": " + path.u8string() +
                " (" + std::error_code(error, std::generic_category()).message() + ")");
        }
    }

    PendingSceneFile::PendingSceneFile(const std::filesystem::path &destination)
        : destination_(std::filesystem::absolute(destination))
    {
        // 随机性不是安全保证：真正防止覆盖其他临时文件的是O_EXCL原子独占创建。
        // 时间戳和进程内计数只减少重名次数，不依赖“先检查存在、再打开”的竞态写法。
        static std::atomic<unsigned long long> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            temporary_ = destination_.parent_path() / (".infinite-scene-" + std::to_string(timestamp) +
                "-" + std::to_string(sequence++) + ".tmp");
#ifdef _WIN32
            const int descriptor = _wopen(temporary_.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                _S_IREAD | _S_IWRITE);
#else
            const int descriptor = ::open(temporary_.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
            if (descriptor < 0)
            {
                const int error = errno;
                if (error == EEXIST) { continue; }
                throw fileError("Cannot create temporary scene file", temporary_, error);
            }
#ifdef _WIN32
            file_ = _fdopen(descriptor, "wb");
#else
            file_ = ::fdopen(descriptor, "wb");
#endif
            if (file_ != nullptr) { return; }
            const int error = errno;
            // 构造失败时析构函数不会执行，所以这里必须关闭原始句柄并回收刚创建的文件。
#ifdef _WIN32
            _close(descriptor);
#else
            ::close(descriptor);
#endif
            std::error_code ignored;
            std::filesystem::remove(temporary_, ignored);
            throw fileError("Cannot open temporary scene stream", temporary_, error);
        }
        throw std::runtime_error("Cannot reserve a unique temporary scene file: " + destination_.u8string());
    }

    PendingSceneFile::~PendingSceneFile()
    {
        if (file_ != nullptr) { std::fclose(file_); }
        if (!committed_)
        {
            // 异常展开时不再抛异常覆盖原始错误。权限或磁盘故障可能导致残留tmp，但不动旧档。
            std::error_code ignored;
            std::filesystem::remove(temporary_, ignored);
        }
    }

    void PendingSceneFile::write(std::string_view text)
    {
        if (file_ == nullptr) { throw std::logic_error("Scene file is already closed"); }
        if (std::fwrite(text.data(), 1, text.size(), file_) != text.size())
        {
            throw fileError("Failed to write temporary scene file", temporary_, errno);
        }
    }

    void PendingSceneFile::commit()
    {
        if (file_ == nullptr) { throw std::logic_error("Scene file is already closed"); }
        if (std::ferror(file_) || std::fflush(file_) != 0)
        {
            throw fileError("Failed to flush temporary scene file", temporary_, errno);
        }
        // 先要求系统刷新文件内容，再发布目录项；不宣称这里实现了目录fsync/断电事务。
#ifdef _WIN32
        const int synced = _commit(_fileno(file_));
#else
        const int synced = ::fsync(::fileno(file_));
#endif
        if (synced != 0) { throw fileError("Failed to sync temporary scene file", temporary_, errno); }
        const int closed = std::fclose(file_);
        file_ = nullptr;
        if (closed != 0) { throw fileError("Failed to close temporary scene file", temporary_, errno); }

#ifdef _WIN32
        // 不使用COPY_ALLOWED：同目录同文件系统直接替换，失败不能退化成先删后拷贝。
        // 宽字符API支持中文路径；被占用且未共享删除权限的目标会明确失败。
        if (!MoveFileExW(temporary_.c_str(), destination_.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            const auto error = GetLastError();
            throw std::runtime_error("Failed to replace scene file: " + destination_.u8string() +
                " (" + std::error_code(error, std::system_category()).message() + ")");
        }
#else
        std::error_code error;
        std::filesystem::rename(temporary_, destination_, error);
        if (error) { throw std::runtime_error("Failed to replace scene file: " + destination_.u8string() + " (" + error.message() + ")"); }
#endif
        committed_ = true;
    }
}
