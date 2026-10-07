#include "UiCanvas.h"

#include "UiButton.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

void UiCanvas::add(std::unique_ptr<UiElement> element)
{
    if (!element)
    {
        throw std::invalid_argument("UiCanvas element must not be null");
    }
    roots_.push_back(std::move(element));
    ++revision_;
}

void UiCanvas::clear() noexcept
{
    roots_.clear();
    ++revision_;
    hovered_ = nullptr;
    pressed_ = nullptr;
    focused_ = nullptr;
}

void UiCanvas::updateHover(UiElement *next)
{
    if (hovered_ == next)
    {
        return;
    }
    if (hovered_)
    {
        hovered_->setHovered(false);
    }
    hovered_ = next;
    if (hovered_)
    {
        hovered_->setHovered(true);
    }
}

std::vector<UiElement *> UiCanvas::focusableElements() const
{
    std::vector<UiElement *> elements;
    for (const auto &root : roots_)
    {
        collectFocusable(*root, elements);
    }
    return elements;
}

void UiCanvas::collectFocusable(const UiElement &element, std::vector<UiElement *> &elements) const
{
    if (!element.isVisible() || !element.isEnabled())
    {
        return;
    }
    if (element.isFocusable())
    {
        elements.push_back(const_cast<UiElement *>(&element));
    }
    for (const auto &child : element.children())
    {
        collectFocusable(*child, elements);
    }
}

void UiCanvas::moveFocus(int direction)
{
    const std::vector<UiElement *> elements = focusableElements();
    if (elements.empty())
    {
        return;
    }

    auto iterator = std::find(elements.begin(), elements.end(), focused_);
    std::size_t index = iterator == elements.end() ?
        (direction > 0 ? elements.size() - 1 : 0) :
        static_cast<std::size_t>(iterator - elements.begin());
    if (direction > 0)
    {
        index = (index + 1) % elements.size();
    }
    else
    {
        index = index == 0 ? elements.size() - 1 : index - 1;
    }

    if (focused_)
    {
        focused_->setFocused(false);
    }
    focused_ = elements[index];
    focused_->setFocused(true);
}

void UiCanvas::processInput(const InputState &input, const glm::vec2 &logicalSize)
{
    size_ = logicalSize;
    const glm::vec2 cursor(static_cast<float>(input.mousePosition().x),
        static_cast<float>(input.mousePosition().y));

    UiElement *nextHovered = nullptr;
    for (auto iterator = roots_.rbegin(); iterator != roots_.rend(); ++iterator)
    {
        if ((nextHovered = (*iterator)->hitTest(cursor, {0.0f, 0.0f})) != nullptr)
        {
            break;
        }
    }
    updateHover(nextHovered);

    if (input.wasKeyPressed(Key::Tab))
    {
        moveFocus(input.isKeyDown(Key::LeftShift) || input.isKeyDown(Key::RightShift) ? -1 : 1);
    }

    if (input.wasMouseButtonPressed(MouseButton::Left))
    {
        pressed_ = hovered_;
        if (pressed_)
        {
            pressed_->setPressed(true);
            if (pressed_->isFocusable())
            {
                if (focused_ && focused_ != pressed_)
                {
                    focused_->setFocused(false);
                }
                focused_ = pressed_;
                focused_->setFocused(true);
            }
        }
    }

    if (input.wasMouseButtonReleased(MouseButton::Left))
    {
        UiElement *released = pressed_;
        if (released)
        {
            released->setPressed(false);
        }
        pressed_ = nullptr;
        if (released && released == hovered_)
        {
            // 激活回调可能清空整个Canvas；调用后不再解引用released。
            const std::size_t revision = revision_;
            released->activate();
            if (revision != revision_)
            {
                hovered_ = nullptr;
                pressed_ = nullptr;
                focused_ = nullptr;
            }
        }
    }

    if (input.wasKeyPressed(Key::Enter) || input.wasKeyPressed(Key::Space))
    {
        UiElement *element = focused_;
        if (element)
        {
            const std::size_t revision = revision_;
            element->activate();
            if (revision != revision_)
            {
                hovered_ = nullptr;
                pressed_ = nullptr;
                focused_ = nullptr;
            }
        }
    }
}

void UiCanvas::collectCommands(UiRenderCommandList &commands) const
{
    commands.clear();
    for (const auto &root : roots_)
    {
        root->collectCommands(commands, {0.0f, 0.0f});
    }
}
