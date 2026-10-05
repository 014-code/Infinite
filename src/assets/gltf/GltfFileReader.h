#pragma once

#include "GltfImportContext.h"

namespace gltf
{
    // 负责把磁盘/data URI/GLB中的字节准备成cgltf可以安全访问的输入。
    // 它不解释材质、网格或节点，也不创建任何OpenGL对象。
    class GltfFileReader final
    {
    public:
        static void read(GltfImportContext &context);

        // 图片依赖已经在Validator确认buffer view合法后再读取，避免损坏的范围
        // 在结构校验之前被解引用。
        static void readImages(GltfImportContext &context);
    };
}
