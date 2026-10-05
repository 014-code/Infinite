#include "TestSupport.h"
#include "assets/GltfLoader.h"
#include "graphics/resources/Texture.h"
#include "platform/Window.h"
#include <iostream>

int main()
{
    try
    {
        const auto model = GltfLoader::load("tests/fixtures/gltf/quad.glb");
        ImageLoadOptions options;
        options.flipVertically = false;
        const auto image = ImageLoader::loadMemory(model.images[0], options, "GLB image 0");
        require(image.width == 2 && image.height == 2 && image.pixels[0] == 255 && image.pixels[2] == 0 &&
            image.pixels[8] == 0 && image.pixels[10] == 255, "glTF PNG orientation changed");
        options.maxDecodedBytes = 4;
        expectThrow<std::runtime_error>([&] { ImageLoader::loadMemory(model.images[0], options); }, "Decoded byte budget ignored");
        expectThrow<std::runtime_error>([] { ImageLoader::loadMemory({1,2,3}); }, "Corrupt memory image accepted");
        Window window(64, 64, "glTF Texture Test", false);
        Texture::SamplingOptions sampling;
        sampling.minFilter = GL_NEAREST; sampling.magFilter = GL_NEAREST;
        sampling.wrapS = GL_CLAMP_TO_EDGE; sampling.wrapT = GL_MIRRORED_REPEAT; sampling.srgb = true;
        Texture texture(image, sampling);
        texture.bind();
        GLint format = 0, wrap = 0, filter = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrap);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
        require(format == GL_SRGB8_ALPHA8 && wrap == GL_MIRRORED_REPEAT && filter == GL_NEAREST,
            "glTF sampling parameters were ignored");
        sampling.magFilter = 999;
        expectThrow<std::invalid_argument>([&] { Texture invalid(image, sampling); }, "Invalid sampling accepted");
        require(glGetError() == GL_NO_ERROR, "Texture upload left GL errors");
        std::cout << "glTF texture memory/orientation/sampling passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
