#include "math/Transform.h"
#include "physics/world/PhysicsWorld.h"
#include "scene/components/AreaComponent.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char *message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }
}

int main()
{
    try
    {
        PhysicsWorld world;
        const PhysicsBodyId body = world.createStaticBody(
            CollisionShape(SphereShape(0.5f)), {0.0f, 0.0f, 0.0f});
        AreaComponent area;
        area.attach(world, CollisionShape(SphereShape(1.0f)));

        int entered = 0;
        int exited = 0;
        area.setBodyEnteredCallback([&](PhysicsBodyId id)
        {
            require(id == body, "Area entered wrong body");
            ++entered;
        });
        area.setBodyExitedCallback([&](PhysicsBodyId id)
        {
            require(id == body, "Area exited wrong body");
            ++exited;
        });

        Transform inside;
        area.poll(inside);
        require(entered == 1 && exited == 0, "Area enter event was not generated");
        area.poll(inside);
        require(entered == 1, "Area generated duplicate enter event");

        inside.position = {5.0f, 0.0f, 0.0f};
        area.poll(inside);
        require(exited == 1, "Area exit event was not generated");
        area.detach();
        require(!area.isAttached(), "Area detach failed");
        std::cout << "Area events passed\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
