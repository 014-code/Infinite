#pragma once

#include <cstdio>
#include <filesystem>
#include <string_view>

namespace scene_serialization
{
    // 存档内部的两阶段写入：先写独占临时文件，commit成功才发布到目标路径。
    // 析构只清理自己创建的临时文件，绝不删除旧档；未提交即离开作用域相当于放弃保存。
    // 不提供跨进程锁，多写入者仍是最后一次成功提交生效；不保证断电恢复和文件元数据保留。
    class PendingSceneFile final
    {
    public:
        explicit PendingSceneFile(const std::filesystem::path &destination);
        ~PendingSceneFile();
        PendingSceneFile(const PendingSceneFile &) = delete;
        PendingSceneFile &operator=(const PendingSceneFile &) = delete;

        void write(std::string_view text);
        // 检查刷新和关闭结果后，再执行同文件系统的替换；不能用“先删除旧档”作为回退。
        void commit();

    private:
        std::filesystem::path destination_;
        std::filesystem::path temporary_;
        std::FILE *file_ = nullptr;
        bool committed_ = false;
    };
}
