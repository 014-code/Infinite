#pragma once

#include <GL/glew.h>

#include "graphics/resources/ImageLoader.h"

class Texture
{
public:
    struct SamplingOptions
    {
        GLint minFilter = GL_LINEAR_MIPMAP_LINEAR;
        GLint magFilter = GL_LINEAR;
        GLint wrapS = GL_REPEAT;
        GLint wrapT = GL_REPEAT;
        bool srgb = false;
    };

    // 文件加载便捷入口：先在CPU解码，再上传RGBA8纹理并生成mipmap。
    // 必须在当前线程拥有有效OpenGL上下文时构造和销毁Texture。
    explicit Texture(const std::filesystem::path &path, const ImageLoadOptions &options = {});

    // 接收预先解码的像素；上传后不保存ImageData引用，调用者可释放CPU数据。
    explicit Texture(const ImageData &image);
    explicit Texture(const ImageData &image, const SamplingOptions &sampling);

    // 对象销毁时释放OpenGL纹理
    ~Texture();

    // Texture拥有OpenGL资源，禁止复制，避免重复释放同一个纹理
    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

    // 允许转移OpenGL纹理的所有权
    Texture(Texture &&other) noexcept;
    Texture &operator=(Texture &&other) noexcept;

    // 将纹理绑定到指定的纹理单元
    void bind(GLuint textureUnit = 0) const;

    bool isSrgb() const noexcept;

private:
    GLuint texture_ = 0;
    bool srgb_ = false;
};
