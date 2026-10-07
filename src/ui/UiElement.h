#pragma once

#include "UiTypes.h"

#include <memory>
#include <utility>
#include <vector>

// UI元素是一个轻量的树节点：父元素负责布局范围，子元素的坐标相对父元素左上角。
// 它不拥有任何OpenGL资源，也不依赖Application，便于菜单和编辑器复用。
class UiElement
{
public:
    virtual ~UiElement() = default;

    UiElement(const UiElement &) = delete;
    UiElement &operator=(const UiElement &) = delete;

    void setRect(const UiRect &rect) noexcept { rect_ = rect; }
    const UiRect &rect() const noexcept { return rect_; }

    void setVisible(bool visible) noexcept { visible_ = visible; }
    bool isVisible() const noexcept { return visible_; }

    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    bool isEnabled() const noexcept { return enabled_; }

    // 添加后由父元素拥有child；返回引用只在child仍属于当前父元素期间有效。
    void addChild(std::unique_ptr<UiElement> child);

    template<class Element, class... Arguments>
    Element &createChild(Arguments &&...arguments)
    {
        auto child = std::make_unique<Element>(std::forward<Arguments>(arguments)...);
        Element &reference = *child;
        addChild(std::move(child));
        return reference;
    }

    const std::vector<std::unique_ptr<UiElement>> &children() const noexcept { return children_; }

    // UiCanvas用这些接口执行递归命中和构建绘制命令。
    UiElement *hitTest(const glm::vec2 &point, const glm::vec2 &parentOrigin) noexcept;
    void collectCommands(UiRenderCommandList &commands, const glm::vec2 &parentOrigin) const;

    virtual bool isFocusable() const noexcept { return false; }
    virtual void setHovered(bool hovered) noexcept { (void)hovered; }
    virtual void setPressed(bool pressed) noexcept { (void)pressed; }
    virtual void setFocused(bool focused) noexcept { (void)focused; }
    virtual void activate() {}

protected:
    UiElement() = default;

    UiRect globalRect(const glm::vec2 &parentOrigin) const noexcept;
    virtual bool isInteractive() const noexcept { return false; }
    virtual void appendCommands(UiRenderCommandList &commands, const UiRect &global) const
    {
        (void)commands;
        (void)global;
    }

private:
    UiRect rect_;
    bool visible_ = true;
    bool enabled_ = true;
    std::vector<std::unique_ptr<UiElement>> children_;
};
