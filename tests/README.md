# 测试维护约定

测试属于验证层，不把测试专用对象或示例布局放进`src/`。

## 如何运行

在仓库根目录执行（本机先将`E:/msys64/ucrt64/bin`加入当前终端PATH）：

```powershell
cmake --build --preset msys2-ucrt64-debug
ctest --preset msys2-ucrt64-debug --output-on-failure
ctest --preset msys2-ucrt64-debug -L cpu --output-on-failure
ctest --preset msys2-ucrt64-debug -R 'shader_uniform|render_preparation|scene_file' -V
```

CPU测试不创建窗口；graphics标签包含隐藏窗口、真实像素测试和示例入口冒烟。不要在缺少驱动时直接跳过像素断言并声称测试通过。

## 公共辅助

- `TestSupport.h`：`require`和`expectThrow`；即使Release定义了NDEBUG，断言仍会执行。
- `support/ColorDepthTarget.h`：固定64×64 RGBA8颜色＋24位深度离屏缓冲；在Window后构造、之前析构。它会改变FBO和viewport，只用于测试准备，不是恢复全部状态的引擎Framebuffer抽象。
- `support/UniformQueryProbe.h`：统计真实uniform驱动查询；临时包装GLEW入口并在析构时恢复，仅限有效上下文、单线程和非嵌套使用。

每个测试仍保留自己的场景、像素期望和异常条件，不用万能测试类隐藏业务断言。不同大小/附件格式的测试无需强行迁入固定离屏缓冲。

## 添加测试

登记位置为`cmake/EngineTests.cmake`，由根CMakeLists包含，所以其中相对路径仍以仓库根目录为基准。

```cmake
add_executable(infinite_new_test tests/new_test.cpp)
configure_engine_test(infinite_new_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/new")
```

Helper统一链接引擎、开启告警、设置运行库PATH、工作目录和30秒超时。额外源码、包含目录、编译宏和夹具依赖继续显式写在登记处。测试产物应写入构建目录；资源编码器的STB实现不要并入引擎。

默认标签是graphics，纯CPU测试要加入文件末尾的cpu清单。图片上传测试继续使用`image_fixtures`依赖，因此单独筛选它也会先准备输入图片。

## 性能数据的边界

`render_preparation_test`使用128个共享资源物体、16层父链和一半透明物体；关闭光栅化，统计预热后100批的uniform查询和5组耗时中位数。

查询次数属于确定性回归：预热后必须为0。耗时只输出、不设通过阈值，因为驱动、Debug/Release、机器负载会影响结果。这不是GPU吞吐或真实游戏帧率测评。

`scene_file_test`覆盖临时写入中断、Windows目标占用、替换失败清理、正常覆盖及中文路径。它不模拟断电、磁盘写满或网络文件系统；不要扩大这些测试的结论。
