#pragma once

#include "ui/font/FontFile.h"

#include <memory>
#include <vector>

namespace UiFont
{
    // FontCollection实现一个简化的Godot式字体回退链。
    // 第一阶段由Application提供主字体，后续可以继续添加中文、Emoji或特殊脚本字体。
    class FontCollection final
    {
    public:
        explicit FontCollection(std::shared_ptr<FontFile> primary);

        void addFallback(std::shared_ptr<FontFile> fallback);
        const FontFile &primary() const noexcept { return *fonts_.front(); }
        const FontFile *fontFor(std::uint32_t codePoint) const noexcept;

    private:
        std::vector<std::shared_ptr<FontFile>> fonts_;
    };
}
