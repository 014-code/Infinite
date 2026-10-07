#include "UiElement.h"

#include <stdexcept>
#include <utility>

void UiElement::addChild(std::unique_ptr<UiElement> child)
{
    if (!child)
    {
        throw std::invalid_argument("UiElement child must not be null");
    }
    children_.push_back(std::move(child));
}

UiRect UiElement::globalRect(const glm::vec2 &parentOrigin) const noexcept
{
    UiRect result = rect_;
    result.x += parentOrigin.x;
    result.y += parentOrigin.y;
    return result;
}

UiElement *UiElement::hitTest(const glm::vec2 &point, const glm::vec2 &parentOrigin) noexcept
{
    if (!visible_ || !enabled_)
    {
        return nullptr;
    }

    const UiRect global = globalRect(parentOrigin);
    // 后添加的子元素在视觉上位于上层，因此反向查找命中目标。
    for (auto iterator = children_.rbegin(); iterator != children_.rend(); ++iterator)
    {
        if (UiElement *hit = (*iterator)->hitTest(point, {global.x, global.y}))
        {
            return hit;
        }
    }
    return isInteractive() && global.contains(point) ? this : nullptr;
}

void UiElement::collectCommands(UiRenderCommandList &commands, const glm::vec2 &parentOrigin) const
{
    if (!visible_)
    {
        return;
    }

    const UiRect global = globalRect(parentOrigin);
    appendCommands(commands, global);
    for (const auto &child : children_)
    {
        child->collectCommands(commands, {global.x, global.y});
    }
}
