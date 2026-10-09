#include "ScenePhysicsSync.h"

#include "scene/Scene.h"
#include "physics/world/PhysicsWorld.h"

void ScenePhysicsSync::syncStatic(const Scene &scene, PhysicsWorld &world)
{
    for (const auto &object : scene.objects_)
    {
        if (object->physicsBody().belongsTo(world) && !object->physicsBody().isDynamic())
        {
            object->physicsBody().syncFromTransform(object->transform);
        }
    }
}

void ScenePhysicsSync::syncDynamic(const Scene &scene, PhysicsWorld &world,
    float interpolationAlpha)
{
    for (const auto &object : scene.objects_)
    {
        if (object->physicsBody().belongsTo(world) && object->physicsBody().isDynamic())
        {
            object->physicsBody().syncToTransform(object->transform, interpolationAlpha);
        }
    }
}

void ScenePhysicsSync::syncCharacters(const Scene &scene)
{
    for (const auto &object : scene.objects_)
    {
        if (object->characterBody().isAttached())
        {
            object->characterBody().syncToTransform(object->transform);
        }
    }
}

void ScenePhysicsSync::updateAreas(const Scene &scene)
{
    for (const auto &object : scene.objects_)
    {
        if (!object->isActive())
        {
            object->area().clearOverlaps();
            continue;
        }
        if (object->area().isAttached())
        {
            object->area().poll(object->transform);
        }
    }
}
