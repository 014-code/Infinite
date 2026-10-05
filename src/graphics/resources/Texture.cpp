#include "graphics/resources/Texture.h"

#include <limits>
#include <stdexcept>

namespace
{
    // 像素上传会读取OpenGL的全局解包状态。保存后设为连续CPU字节布局，
    // 防止其他代码的PBO、行长度或跳过行数导致图片错位，析构时恢复原状态。
    class TextureUploadState
    {
    public:
        TextureUploadState()
        {
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_);
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer_);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment_);
            glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength_);
            glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows_);
            glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels_);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        }

        ~TextureUploadState()
        {
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_));
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(buffer_));
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignment_);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength_);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows_);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels_);
        }

        TextureUploadState(const TextureUploadState &) = delete;
        TextureUploadState &operator=(const TextureUploadState &) = delete;

    private:
        GLint texture_ = 0;
        GLint buffer_ = 0;
        GLint alignment_ = 0;
        GLint rowLength_ = 0;
        GLint skipRows_ = 0;
        GLint skipPixels_ = 0;
    };
}

// 委托构造：图片格式解析全部交给ImageLoader，上传逻辑只实现一份。
Texture::Texture(const std::filesystem::path &path, const ImageLoadOptions &options)
    : Texture(ImageLoader::load(path, options))
{
}

Texture::Texture(const ImageData &image)
    : Texture(image, {})
{
}

Texture::Texture(const ImageData &image, const SamplingOptions &sampling)
    : srgb_(sampling.srgb)
{
    // 先验证采样配置再创建GPU句柄，非法配置不会泄漏纹理。
    if (sampling.minFilter != GL_NEAREST && sampling.minFilter != GL_LINEAR &&
        sampling.minFilter != GL_NEAREST_MIPMAP_NEAREST && sampling.minFilter != GL_LINEAR_MIPMAP_NEAREST &&
        sampling.minFilter != GL_NEAREST_MIPMAP_LINEAR && sampling.minFilter != GL_LINEAR_MIPMAP_LINEAR)
    { throw std::invalid_argument("Invalid Texture minification filter"); }
    if (sampling.magFilter != GL_NEAREST && sampling.magFilter != GL_LINEAR)
    { throw std::invalid_argument("Invalid Texture magnification filter"); }
    const auto validWrap = [](GLint value)
    { return value == GL_REPEAT || value == GL_MIRRORED_REPEAT || value == GL_CLAMP_TO_EDGE; };
    if (!validWrap(sampling.wrapS) || !validWrap(sampling.wrapT))
    { throw std::invalid_argument("Invalid Texture wrap mode"); }
    // 这里检查GPU上限；ImageLoader中的限制仅用于CPU解码，两者职责不同。
    GLint maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    if (image.width <= 0 || image.height <= 0 ||
        image.width > maximumTextureSize || image.height > maximumTextureSize)
    {
        throw std::runtime_error("Image dimensions exceed the GPU texture size limit");
    }

    // 公开的ImageData也可以由调用者手动构造，所以上传前仍要验证像素数量。
    const size_t width = static_cast<size_t>(image.width);
    const size_t height = static_cast<size_t>(image.height);
    if (width > std::numeric_limits<size_t>::max() / ImageData::channels ||
        height > std::numeric_limits<size_t>::max() / (width * ImageData::channels) ||
        image.pixels.size() != width * height * ImageData::channels)
    {
        throw std::invalid_argument("ImageData must contain width * height * 4 RGBA bytes");
    }

    TextureUploadState savedState;
    glGenTextures(1, &texture_);
    if (texture_ == 0)
    {
        throw std::runtime_error("Failed to create OpenGL texture");
    }
    glBindTexture(GL_TEXTURE_2D, texture_);
    // sRGB纹理上传后，采样器会自动把颜色转换到线性空间；Alpha仍保持线性。
    // 旧构造函数默认使用RGBA8，确保现有示例的显示行为不变。
    const GLint internalFormat = sampling.srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height,
        0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());

    // mipmap保存逐级缩小的图片。远处物体使用较小层级，减少缩小时的闪烁。
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampling.wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampling.wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampling.minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampling.magFilter);

    // 验证基础层和最小mipmap层已分配，避免返回一个上传失败的纹理对象。
    int lastLevel = 0;
    for (int size = image.width > image.height ? image.width : image.height; size > 1; size /= 2)
    {
        ++lastLevel;
    }
    GLint uploadedWidth = 0;
    GLint mipWidth = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &uploadedWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, lastLevel, GL_TEXTURE_WIDTH, &mipWidth);
    if (uploadedWidth != image.width || mipWidth != 1)
    {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
        throw std::runtime_error("Failed to upload texture or generate mipmaps");
    }
    // savedState在离开作用域时恢复状态；上传期间不改变活动纹理单元。
}

// 对象离开作用域时自动释放GPU纹理，必须在窗口关闭之前执行。
Texture::~Texture()
{
    if (texture_ != 0)
    {
        glDeleteTextures(1, &texture_);
    }
}

// 移动构造：接管原对象的纹理句柄，不复制GPU中的像素。
Texture::Texture(Texture &&other) noexcept
    : texture_(other.texture_), srgb_(other.srgb_)
{
    // 清空原对象的句柄，防止重复释放纹理。
    other.texture_ = 0;
    other.srgb_ = false;
}

// 移动赋值：先释放当前纹理，再接管原对象的纹理。
Texture &Texture::operator=(Texture &&other) noexcept
{
    if (this != &other)
    {
        if (texture_ != 0)
        {
            glDeleteTextures(1, &texture_);
        }
        // 转移纹理资源所有权。
        texture_ = other.texture_;
        srgb_ = other.srgb_;
        other.texture_ = 0;
        other.srgb_ = false;
    }
    return *this;
}

void Texture::bind(GLuint textureUnit) const
{
    // 激活纹理单元，并绑定当前纹理。
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, texture_);
}

bool Texture::isSrgb() const noexcept
{
    return srgb_;
}
