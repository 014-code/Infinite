#include "../common/ExampleRun.h"

// 这是可以直接编译运行的空白模板。它不包含三角形、立方体或任何默认场景物体。
// 新建示例时可复制此文件，再按下面两个位置补充自己的资源与逻辑。
int main(int argc, char *argv[])
{
    // runExample只负责示例的启动日志、Escape和冒烟选项；真正主循环在Application中。
    return runExample(argc, argv, "application_template", "Infinite - Application Template",
        {0.08f, 0.10f, 0.15f, 1.0f}, [](Application &application, const std::filesystem::path &directory)
    {
        // 1. 初始化：窗口上下文已就绪，可从directory加载Shader/图片，再创建场景物体。
        //    推荐make_shared创建资源，通过setRenderable交给物体持有，不能借用这里的局部GPU对象。
        //    Application会在这个初始化回调返回后开始计时，不必自己创建Time。

        // 2. 每帧逻辑：给物体注册setUpdateCallback，用传入的deltaTime控制运动。
        //    不需要自己调用pollEvents、Scene::update、clear、render或swapBuffers。

        // 模板没有加载资源，也没有场景物体；显式标记未用参数，避免编译警告。
        (void)application;
        (void)directory;
    }, ExampleFrameContent::ClearOnly);
    // 添加可见物体后，将ClearOnly改成DrawnScene，冒烟测试就会检查实际绘制内容。
}
