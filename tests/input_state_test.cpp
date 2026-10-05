#include "TestSupport.h"
#include "input/InputState.h"

#include <iostream>

int main()
{
    try
    {
        InputState state;
        state.keyEvent(Key::W, true);
        require(state.isKeyDown(Key::W) && state.wasKeyPressed(Key::W), "Missing press");
        state.beginFrame();
        state.keyEvent(Key::W, true);
        require(state.isKeyDown(Key::W) && !state.wasKeyPressed(Key::W), "Repeat created edge");
        state.keyEvent(Key::W, false);
        require(state.wasKeyReleased(Key::W) && !state.isKeyDown(Key::W), "Missing release");
        state.beginFrame();
        state.keyEvent(Key::Space, true);
        state.keyEvent(Key::Space, false);
        require(state.wasKeyPressed(Key::Space) && state.wasKeyReleased(Key::Space), "Fast tap lost");
        state.mouseButtonEvent(MouseButton::Left, true);
        state.cursorEvent(100, 200);
        require(state.mouseDelta() == glm::dvec2(0), "First cursor event jumped");
        state.cursorEvent(102, 204);
        state.scrollEvent(1, 2);
        state.scrollEvent(0, 3);
        require(state.mouseDelta() == glm::dvec2(2, 4) && state.scrollDelta() == glm::dvec2(1, 5), "Delta not accumulated");
        state.keyEvent(Key::A, true);
        state.focusLost();
        require(!state.isKeyDown(Key::A) && state.wasKeyReleased(Key::A), "Focus left stuck key");
        require(!state.isMouseButtonDown(MouseButton::Left) && state.wasMouseButtonReleased(MouseButton::Left), "Focus left stuck mouse");
        state.cursorEvent(500, 500);
        require(state.mouseDelta() == glm::dvec2(0), "Refocus jumped");
        state.beginFrame();
        require(!state.wasKeyReleased(Key::A) && state.scrollDelta() == glm::dvec2(0), "Frame not reset");
        state.keyEvent(static_cast<Key>(-1), true);
        require(!state.isKeyDown(static_cast<Key>(-1)), "Invalid key accepted");
        std::cout << "Input state passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
