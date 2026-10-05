#include <GL/glew.h>
#include "TestSupport.h"
#include "support/ColorDepthTarget.h"
#include "graphics/camera/Camera.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/rendering/HdrPipeline.h"
#include "animation/AnimationPlayer.h"
#include "animation/AnimationBlender.h"
#include "animation/AnimationStateMachine.h"
#include "platform/Window.h"
#include "resources/ResourceManager.h"
#include "scene/Scene.h"
#include <iostream>
#include <array>

int main()
{
    try
    {
        // 先验证纯CPU混合器，再验证它通过播放器/状态机写入实际实例。
        std::vector<LocalPose> blendFrom(1), blendTo(1);
        blendFrom[0].position = {0, 0, 0};
        blendTo[0].position = {4, 2, 0};
        blendTo[0].rotation = {0, 0, 1, 0};
        const auto blended = AnimationBlender::blend(blendFrom, blendTo, .5f);
        require(blended[0].position == glm::vec3(2, 1, 0) &&
            std::abs(glm::length(blended[0].rotation) - 1) < .0001f,
            "Animation pose blend did not interpolate TRS");
        expectThrow<std::invalid_argument>([&] { AnimationBlender::blend(blendFrom, blendTo, 2); },
            "Invalid blend weight accepted");

        Window window(64, 64, "Animation player test", false);
        ResourceManager resources;
        auto model = resources.loadPbrModel("tests/fixtures/animation/rigid.glb");
        Scene scene;
        auto first = ModelInstantiator::instantiate(scene, *model);
        auto second = ModelInstantiator::instantiate(scene, *model);
        AnimationPlayer a(scene, model, first), b(scene, model, second);
        AnimationStateMachine machine(scene, model, first);
        machine.addState("float", 0);
        machine.addState("spin", 1);
        expectThrow<std::invalid_argument>([&] { machine.addState("float", 0); },
            "Duplicate animation state accepted");
        machine.setInitialState(scene, "float");
        machine.transitionTo(scene, "spin", 1.0f);
        machine.update(scene, .5f);
        require(machine.state() == "spin" && machine.player().isSelected() &&
            machine.player().isPlaying(), "Animation state transition did not start");
        machine.update(scene, .5f);
        expectThrow<std::invalid_argument>([&] { machine.transitionTo(scene, "missing", 1); },
            "Unknown animation state accepted");
        auto &root = scene.findObject(first.rootId)->transform;
        root.position.x = 20;
        a.play(scene, 0); b.play(scene, 0);
        a.update(scene, 1); b.setSpeed(2); b.update(scene, 1);
        auto &poseA = scene.findObject(first.nodeIds[1])->transform;
        auto &poseB = scene.findObject(second.nodeIds[1])->transform;
        require(poseA.position.y == 1.5f && poseB.position.y == 2 && root.position.x == 20,
            "Instances shared playback state or moved instance root");
        a.pause(); a.update(scene, 1);
        require(a.time() == 1, "Pause advanced clock");
        a.resume(); a.update(scene, 3);
        require(a.time() == 0 && poseA.position.y == 1, "Loop boundary mismatch");
        a.setLooping(false); a.update(scene, 20);
        require(a.time() == 4 && !a.isPlaying(), "Non-looping clip did not hold last key");
        a.seek(scene, 2); require(poseA.position.y == 2, "Seek failed");
        a.play(scene, 1); require(poseA.position.y == 1, "Switching clips retained stale translation");
        a.stop(scene);
        require(a.time() == 0 && !a.isPlaying() && poseA.rotation().w == 1 && root.position.x == 20,
            "Stop did not restore bind pose only");
        // 不只验证CPU数值：同一渲染路径在两个确定时间应产生不同画面。
        ColorDepthTarget target;
        Camera camera; camera.setOrthographic(6, .1f, 100);
        Renderer renderer; HdrPipeline hdr;
        root.position.x = 0;
        scene.findObject(second.rootId)->transform.position.x = 20;
        a.play(scene, 0);
        std::array<unsigned char, 64 * 64 * 4> before{}, after{};
        hdr.render(64, 64, {0,0,0,1}, [&] { scene.render(renderer, camera, 1); });
        glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, before.data());
        a.seek(scene, 1);
        hdr.render(64, 64, {0,0,0,1}, [&] { scene.render(renderer, camera, 1); });
        glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, after.data());
        int changed = 0;
        for (std::size_t pixel = 0; pixel < before.size(); pixel += 4)
        { if (before[pixel] != after[pixel] || before[pixel+1] != after[pixel+1]) { ++changed; } }
        require(changed > 20, "Animated model did not visibly move");
        expectThrow<std::invalid_argument>([&] { a.setSpeed(-1); }, "Negative speed accepted");
        Scene wrong;
        expectThrow<std::invalid_argument>([&] { AnimationPlayer invalid(wrong, model, first); }, "Wrong Scene accepted");
        expectThrow<std::runtime_error>([&] { a.play(wrong, 0); }, "Playback wrote to wrong Scene");
        resources.clear();
        a.play(scene, 0); a.seek(scene, 1);
        scene.removeObject(first.nodeIds.back());
        expectThrow<std::runtime_error>([&] { a.update(scene, 1); }, "Deleted node silently ignored");
        require(a.time() == 1 && poseA.position.y == 1.5f, "Failed playback partially updated clock/pose");
        ModelInstantiator::remove(scene, second);
        expectThrow<std::runtime_error>([&] { b.update(scene, 1); }, "Removed instance accepted");
        require(glGetError() == GL_NO_ERROR, "Animation integration caused GL error");
        std::cout << "Animation player: independent instances, loop, pause, seek, switch, stop and deletion passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
