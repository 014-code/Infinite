#include "TestSupport.h"
#include "graphics/resources/Texture.h"
#include "platform/Window.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace
{
    GLint state(GLenum name)
    {
        GLint value = 0;
        glGetIntegerv(name, &value);
        return value;
    }
}

int main(int argc, char *argv[])
{
    try
    {
        require(argc == 2, "Expected PNG fixture path");
        // 先解码再创建窗口，证明CPU加载与GPU资源生命周期可以分离。
        const auto image = ImageLoader::load(argv[1]);
        Window window(64, 64, "Texture Upload Test", false);
        Texture original(image);
        original.bind(3);
        const GLint oldTexture = state(GL_TEXTURE_BINDING_2D);

        // 人为设置非默认解包状态，确认Texture会正确上传并恢复调用者状态。
        GLuint unpackBuffer = 0;
        glGenBuffers(1, &unpackBuffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 128, nullptr, GL_STATIC_DRAW);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 7);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 2);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
        Texture texture(argv[1]);
        require(state(GL_ACTIVE_TEXTURE) == GL_TEXTURE3 && state(GL_TEXTURE_BINDING_2D) == oldTexture,
            "Texture creation changed caller binding or texture unit");
        require(state(GL_PIXEL_UNPACK_BUFFER_BINDING) == static_cast<GLint>(unpackBuffer) &&
            state(GL_UNPACK_ALIGNMENT) == 8 && state(GL_UNPACK_ROW_LENGTH) == 7 &&
            state(GL_UNPACK_SKIP_ROWS) == 2 && state(GL_UNPACK_SKIP_PIXELS) == 1,
            "Pixel unpack state was not restored");
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glDeleteBuffers(1, &unpackBuffer);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

        texture.bind();
        GLint format = 0;
        GLint filter = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
        require(format == GL_RGBA8 && filter == GL_LINEAR_MIPMAP_LINEAR, "Incorrect texture format or filtering");

        // 读回全部RGBA，而不是只检查纹理句柄，确保透明度与图片方向真正上传正确。
        std::array<unsigned char, 16> pixels{};
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        require(std::vector<unsigned char>(pixels.begin(), pixels.end()) == image.pixels,
            "Uploaded colors, orientation or alpha incorrect");
        GLint mipWidth = 0;
        GLint mipHeight = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &mipWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_HEIGHT, &mipHeight);
        require(mipWidth == 1 && mipHeight == 1, "Missing mipmap level");

        // move之后资源仍可使用；离开作用域时只由新的持有者释放。
        Texture moved(std::move(texture));
        Texture assigned(image);
        assigned = std::move(moved);
        assigned.bind();
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        require(std::vector<unsigned char>(pixels.begin(), pixels.end()) == image.pixels,
            "Move lost the GPU texture");

        ImageData invalid = image;
        invalid.pixels.pop_back();
        bool rejected = false;
        try
        {
            Texture invalidTexture(invalid);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        require(rejected, "Invalid CPU buffer was not rejected");
        require(glGetError() == GL_NO_ERROR, "OpenGL error during upload test");
        std::cout << "Texture upload tests passed: RGBA, mipmap, state and moves" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Texture upload test failed: " << exception.what() << std::endl;
        return 1;
    }
}
