#include "GltfAnimationDecoder.h"

#include <string>
#include <utility>

namespace gltf
{
    namespace
    {
        AnimationChannel decodeChannel(const cgltf_animation_channel &source,
            GltfImportContext &context, std::size_t &keysLeft)
        {
            require(source.target_node && source.sampler, "Missing target node or sampler");
            require(!source.target_node->has_matrix, "Animated node must use TRS, not matrix");
            AnimationChannel result;
            const auto node = cgltf_node_index(context.document.get(), source.target_node);
            require(node < context.nodeMapping.size(), "Invalid target node");
            const auto &sampler = *source.sampler;
            require(sampler.interpolation == cgltf_interpolation_type_linear ||
                sampler.interpolation == cgltf_interpolation_type_step,
                "Only STEP and LINEAR animation are supported (CUBICSPLINE is not approximated)");
            result.interpolation = sampler.interpolation == cgltf_interpolation_type_step ?
                AnimationInterpolation::Step : AnimationInterpolation::Linear;
            switch (source.target_path)
            {
            case cgltf_animation_path_type_translation: result.path = AnimationPath::Translation; break;
            case cgltf_animation_path_type_rotation: result.path = AnimationPath::Rotation; break;
            case cgltf_animation_path_type_scale: result.path = AnimationPath::Scale; break;
            default: throw std::runtime_error("Morph/unknown animation path is not supported");
            }
            const auto *input = sampler.input;
            const auto *output = sampler.output;
            const bool rotation = result.path == AnimationPath::Rotation;
            require(input && output && input->type == cgltf_type_scalar &&
                input->component_type == cgltf_component_type_r_32f && !input->normalized &&
                output->type == (rotation ? cgltf_type_vec4 : cgltf_type_vec3) &&
                output->component_type == cgltf_component_type_r_32f && !output->normalized &&
                input->count == output->count && input->count > 0,
                "Invalid animation input/output accessor type or count");
            require(input->count <= keysLeft, "Animation key count limit exceeded");
            keysLeft -= input->count;
            context.consumeArray<float>(input->count, "animation times");
            context.consumeArray<glm::vec4>(input->count, "animation values");
            result.times.resize(input->count);
            result.values.resize(input->count, glm::vec4(0));
            for (std::size_t key = 0; key < input->count; ++key)
            {
                require(cgltf_accessor_read_float(input, key, &result.times[key], 1) &&
                    cgltf_accessor_read_float(output, key, &result.values[key][0], rotation ? 4 : 3),
                    "Failed to read animation key");
            }
            // 不在所选scene中的通道也先校验，不能让坏数据躲过验证。
            result.node = node;
            return result;
        }
    }

    void GltfAnimationDecoder::decode(GltfImportContext &context)
    {
        const auto &data = *context.document;
        context.consumeArray<AnimationClip>(data.animations_count, "animation clips");
        context.result.animations.reserve(data.animations_count);
        std::size_t keysLeft = context.options.maxAnimationKeys;
        for (std::size_t index = 0; index < data.animations_count; ++index)
        {
            const auto &source = data.animations[index];
            const std::string prefix = "Animation[" + std::to_string(index) + "] ";
            context.consumeArray<AnimationChannel>(source.channels_count, "animation channels");
            std::vector<AnimationChannel> channels;
            channels.reserve(source.channels_count);
            for (std::size_t channel = 0; channel < source.channels_count; ++channel)
            {
                try { channels.push_back(decodeChannel(source.channels[channel], context, keysLeft)); }
                catch (const std::exception &error)
                { throw std::runtime_error(prefix + "Channel[" + std::to_string(channel) + "] " + error.what()); }
            }
            const std::string name = source.name ? source.name : "animation_" + std::to_string(index);
            context.consume(name.size(), "animation name");
            try
            {
                // 先验证原始通道（包含重复target和关键帧），再过滤当前scene以外的节点。
                AnimationClip validated(name, std::move(channels));
                context.consumeArray<AnimationChannel>(validated.channels().size(), "mapped animation channels");
                std::vector<AnimationChannel> mapped;
                for (const auto &channel : validated.channels())
                {
                    const int target = context.nodeMapping.at(channel.node);
                    if (target < 0)
                    {
                        context.result.warnings.push_back(prefix + "channel targets a node outside the selected scene");
                        continue;
                    }
                    context.consumeArray<float>(channel.times.size(), "mapped animation times");
                    context.consumeArray<glm::vec4>(channel.values.size(), "mapped animation values");
                    mapped.push_back(channel);
                    mapped.back().node = static_cast<std::size_t>(target);
                }
                context.result.animations.emplace_back(name, std::move(mapped));
            }
            catch (const std::exception &error) { throw std::runtime_error(prefix + error.what()); }
        }
    }
}
