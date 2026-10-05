#include "TestSupport.h"
#include "animation/AnimationSampler.h"
#include "assets/GltfLoader.h"
#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    try
    {
        AnimationChannel position{0, AnimationPath::Translation, AnimationInterpolation::Linear,
            {1, 3}, {{0, 0, 0, 0}, {4, 2, 0, 0}}};
        AnimationClip linear("linear", {position});
        std::vector<LocalPose> rest(2);
        rest[1].position.x = 9;
        const auto mid = AnimationSampler::sample(linear, 2, rest);
        require(mid[0].position.x == 2 && mid[1].position.x == 9, "Linear/rest pose mismatch");
        require(AnimationSampler::sample(linear, 0, rest)[0].position.x == 0 &&
            AnimationSampler::sample(linear, 10, rest)[0].position.x == 4, "Endpoint clamping mismatch");
        position.interpolation = AnimationInterpolation::Step;
        AnimationClip step("step", {position});
        require(AnimationSampler::sample(step, 2.999, rest)[0].position.x == 0 &&
            AnimationSampler::sample(step, 3, rest)[0].position.x == 4, "STEP boundary mismatch");
        AnimationChannel rotation{0, AnimationPath::Rotation, AnimationInterpolation::Linear,
            {0, 2}, {{0, 0, 0, 1}, {0, 0, -.70710678f, -.70710678f}}};
        AnimationClip spin("short path", {rotation});
        auto q = AnimationSampler::sample(spin, 1, rest)[0].rotation;
        require(std::abs(q.z - .3826834f) < .0001f && std::abs(q.w - .9238795f) < .0001f,
            "Quaternion did not take shortest path");
        rotation.values[1] = {0, 0, 0, -1};
        require(std::abs(AnimationSampler::sample(AnimationClip("same", {rotation}), 1, rest)[0].rotation.w) > .9999f,
            "Antipodal quaternion should not rotate");
        AnimationChannel single{0, AnimationPath::Scale, AnimationInterpolation::Linear, {0}, {{2,2,2,0}}};
        require(AnimationSampler::sample(AnimationClip("single", {single}), 100, rest)[0].scale.x == 2, "Single key mismatch");
        expectThrow<std::invalid_argument>([&] { AnimationClip("duplicate", {position, position}); }, "Duplicate target accepted");
        auto bad = position; bad.times = {1, 1};
        expectThrow<std::invalid_argument>([&] { AnimationClip("bad", {bad}); }, "Duplicate time accepted");
        bad = position; bad.values[0].x = std::numeric_limits<float>::infinity();
        expectThrow<std::invalid_argument>([&] { AnimationClip("bad", {bad}); }, "Nonfinite value accepted");
        expectThrow<std::invalid_argument>([&] { AnimationSampler::sample(linear, -1, rest); }, "Negative time accepted");
        expectThrow<std::out_of_range>([&] { AnimationSampler::sample(linear, 1, {}); }, "Missing pose node accepted");

        auto data = GltfLoader::load("tests/fixtures/animation/rigid.glb");
        require(data.animations.size() == 2 && data.animations[0].duration() == 4, "glTF clip list/duration lost");
        require(data.nodes[0].name == "assembly" && data.animations[0].channels()[0].node == 1 &&
            data.animations[0].channels()[2].node == 2, "Source to scene node remapping failed");
        for (const auto *name : {"cubic", "morph", "duplicate", "matrix", "count", "bad-time", "bad-quat"})
        {
            expectThrow<std::runtime_error>([&] {
                GltfLoader::load(std::filesystem::path("tests/fixtures/animation") / (std::string(name) + ".glb"));
            }, "Invalid animation fixture accepted");
        }
        ModelLoadOptions limits; limits.maxAnimationKeys = 3;
        expectThrow<std::runtime_error>([&] { GltfLoader::load("tests/fixtures/animation/rigid.glb", limits); }, "Key budget ignored");
        std::cout << "Animation CPU: TRS, STEP, shortest-path rotation, bounds, remapping and malformed glTF passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
