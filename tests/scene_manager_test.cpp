#include "resources/ResourceManager.h"
#include "scene/Scene.h"
#include "scene/SceneManager.h"

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
        ResourceManager resources;
        Scene scene;
        SceneManager manager(scene, resources);
        manager.registerScene("menu", [](Scene &target, ResourceManager &)
        {
            target.createObject("MenuRoot");
        });
        manager.registerScene("game", [](Scene &target, ResourceManager &)
        {
            target.createObject("Player");
            target.createObject("Ground");
        });
        manager.registerScene("broken", [](Scene &target, ResourceManager &)
        {
            target.createObject("HalfLoaded");
            throw std::runtime_error("test loader failure");
        });

        manager.load("menu");
        require(manager.currentName() == "menu" && scene.objectCount() == 1,
            "Initial scene load failed");

        manager.requestLoad("game");
        require(manager.hasPendingLoad(), "Scene load request was not queued");
        require(manager.commitPending(), "Pending scene was not committed");
        require(manager.currentName() == "game" && scene.objectCount() == 2,
            "Pending scene transition failed");

        bool failed = false;
        try { manager.load("broken"); }
        catch (const std::runtime_error &) { failed = true; }
        require(failed && manager.currentName().empty() && scene.objectCount() == 0,
            "Failed scene load left a partial scene");

        manager.unload();
        require(scene.objectCount() == 0 && manager.currentName().empty(),
            "Scene unload failed");
        std::cout << "Scene manager passed\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
