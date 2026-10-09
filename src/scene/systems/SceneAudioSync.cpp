#include "SceneAudioSync.h"

#include "scene/Scene.h"

void SceneAudioSync::sync(const Scene &scene)
{
    for (const auto &object : scene.objects_)
    {
        if (object->isActive())
        {
            object->audioSource().syncTransform(object->transform);
        }
    }
}
