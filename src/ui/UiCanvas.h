#pragma once

#include "UiElement.h"

#include "input/InputState.h"

#include <glm/vec2.hpp>
#include <memory>
#include <cstddef>
#include <vector>

// UiCanvas是UI树和输入焦点的所有者。它不负责GPU绘制，由Application统一调用UiRenderer。
class UiCanvas final
{
public:
    UiCanvas() = default;
    ~UiCanvas() = default;

    UiCanvas(const UiCanvas &) = delete;
    UiCanvas &operator=(const UiCanvas &) = delete;

    void setSize(const glm::vec2 &size) noexcept { size_ = size; }
    const glm::vec2 &size() const noexcept { return size_; }

    void add(std::unique_ptr<UiElement> element);

    template<class Element, class... Arguments>
    Element &create(Arguments &&...arguments)
    {
        auto element = std::make_unique<Element>(std::forward<Arguments>(arguments)...);
        Element &reference = *element;
        add(std::move(element));
        return reference;
    }

    void clear() noexcept;
    bool empty() const noexcept { return roots_.empty(); }

    // 每帧在Application事件回调后调用一次；坐标使用窗口逻辑像素。
    // 输入只触发按钮回调，不会自动修改Scene或Application状态。
    void processInput(const InputState &input, const glm::vec2 &logicalSize);

    // UiRenderer使用的只读绘制入口。
    void collectCommands(UiRenderCommandList &commands) const;

    UiElement *focusedElement() const noexcept { return focused_; }
    UiElement *hoveredElement() const noexcept { return hovered_; }

private:
    void updateHover(UiElement *next);
    void moveFocus(int direction);
    std::vector<UiElement *> focusableElements() const;
    void collectFocusable(const UiElement &element, std::vector<UiElement *> &elements) const;

    glm::vec2 size_{0.0f};
    std::vector<std::unique_ptr<UiElement>> roots_;
    UiElement *hovered_ = nullptr;
    UiElement *pressed_ = nullptr;
    UiElement *focused_ = nullptr;
    // 激活回调可能清空/重建Canvas；版本号用于避免回调返回后继续使用悬空控件指针。
    std::size_t revision_ = 0;
};
