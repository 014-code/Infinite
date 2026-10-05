# Infinite

一个使用 C++、OpenGL 和 GLFW 编写的游戏引擎学习项目。

功能总览、对应示例及尚不支持的边界见[项目功能清单](docs/项目功能清单.md)。先从这份索引了解框架，再按需阅读专题说明。

本轮维护内容与测量记录见[维护优化记录](docs/维护优化记录.md)，新增测试的方式见[测试维护约定](tests/README.md)。

最新示例：运行 `runtime/examples/material_showcase/material_showcase.exe`，查看三个真实glTF模型与基础颜色贴图，详见 [形状与材质展示说明](docs/形状与材质展示说明.md)。
glTF基础颜色预览运行 `runtime/examples/gltf_model/gltf_model.exe`，详见 [glTF导入说明](docs/gltf导入说明.md)。
新增PBR、节点动画、骨骼和方向光阴影示例见 [PBR动画与阴影使用说明](docs/PBR动画与阴影使用说明.md)；这不等于完整glTF扩展支持。
四元数、路径规则与场景加载说明见 [文件资源与场景示例](docs/文件资源与场景示例.md)。

## 模块结构

```text
src/
├── core/       # 应用运行层、帧时间、资源路径、统一日志
├── animation/  # 动画片段、TRS采样、播放器、蒙皮矩阵
├── assets/     # OBJ网格、Material文件与glTF模型的CPU解析
├── animation/  # 动画片段、TRS采样、实例播放器、蒙皮矩阵
├── input/      # 键盘、鼠标及逐帧输入状态
├── math/       # 四元数旋转与父子变换
├── platform/   # 窗口和OpenGL上下文
├── scene/      # 游戏对象、组件、对象所有权、场景管理和场景序列化
├── resources/  # Shader、Texture、Mesh、Material和Model缓存
└── graphics/
    ├── camera/     # Camera
    ├── lighting/   # SceneLighting、方向光、点光源和聚光灯
    ├── geometry/   # Mesh、Vertex
    ├── resources/  # Shader、Texture、Material、ImageLoader
    └── rendering/  # Renderer、RenderItem、HDR与Shadow Map、OpenGLDebug
examples/       # 可独立运行的功能示例及其Shader、资源
tests/          # 自动化冒烟测试
runtime/        # 各示例的可执行文件、资源和运行库
```

当前示例：

- `color_triangle`：彩色三角形，演示Mesh、Shader、Transform和输入。
- `textured_quad`：带纹理的矩形，演示Texture和纹理渲染。
- `textured_cube`：带纹理的旋转立方体，演示深度测试和背面剔除。
- `diffuse_lighting`：纯色立方体的方向光漫反射、环境光、法线变换与自由摄像机。
- `player_controller`：示例层的平地移动演示，显式按键映射→移动指令→平移/跳跃，朝向和重生由应用决定；固定高度不是真实碰撞。详见[玩家控制器说明](docs/玩家控制器说明.md)。
- `alpha_blending`：用程序生成的半透明圆形贴图演示透明混合、远到近排序和深度写入控制。
- `scene_objects`：由Scene统一管理多个物体，演示共享资源、独立变换、透明排序和材质剔除配置。
- `asset_scene`：从场景文件自动加载OBJ和Material，Material继续加载Shader与纹理；演示资源缓存复用、父子Transform和四元数旋转。
- `gltf_model`：加载多节点、多primitive的静态glTF，演示内嵌/外部资源、基础颜色纹理和多个独立实例。
- `material_showcase`：展示三个下载的真实glTF模型、基础颜色贴图及方向光漫反射，不是完整PBR。
- `application_template`：可直接复制的空白应用模板，不创建任何默认物体。
- `primitive_shapes`：使用引擎内置创建接口生成平面、立方体、圆盘、球体、圆柱和圆锥，不手写顶点数据。
- `scene_lighting`：场景级环境光、多方向光、点光源和聚光灯，演示光照与材质职责分离。

新增独立示例：`pbr_materials`（材质对照）、`gltf_pbr`（原版Avocado）、`node_animation`（刚体动画）、`skeletal_animation`（Rigged Figure双实例）、`directional_shadow`（主方向光投影）。

场景光照的接口、限制和测试见[场景光照说明](docs/场景光照说明.md)；阶段状态见[渲染与动画实施计划](docs/渲染与动画实施计划.md)。

立方体材质设为 `CullMode::Back`，约定屏幕上逆时针绕序为正面、剔除背面。
Scene渲染时会按每个Material配置深度测试、混合和剔除状态；三角形和纹理矩形默认不启用面剔除。
单轴负缩放会反转绕序；启用Material的 `correctMirroredWinding` 后，Scene会按最终世界矩阵自动调整正面绕序，双面图片也可以关闭剔除。Scene会在绘制结束后恢复进入时的OpenGL状态。
立方体顶点数据位于 `examples/textured_cube/CubeGeometry.h`，由示例及回归测试共用，不属于引擎模块。

渲染层的Shader、Material、SceneLighting、颜色空间和Renderer状态约定见[渲染接口约定](docs/渲染接口约定.md)。

## 编译环境

项目使用 MSYS2 UCRT64。安装编译器、CMake、Ninja 和图形依赖：

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-glm mingw-w64-ucrt-x86_64-stb
```

在项目根目录构建。推荐使用仓库内的Preset，它固定使用E盘MSYS2 UCRT64工具链，并把编译并发限制为1：

```powershell
cmake --preset msys2-ucrt64-debug
cmake --build --preset msys2-ucrt64-debug
ctest --preset msys2-ucrt64-debug
```

首次使用前，PowerShell需要能找到CMake。可以先在当前终端执行 `$env:PATH = 'E:\msys64\ucrt64\bin;' + $env:PATH`，或用 `E:/msys64/ucrt64/bin/cmake.exe`、`E:/msys64/ucrt64/bin/ctest.exe` 替代上述命令名。此设置不会修改系统PATH。

Preset假设MSYS2位于 `E:\msys64`；若以后移动安装目录，可在被Git忽略的 `CMakeUserPresets.json` 中新增继承此配置的Preset，覆盖环境变量 `INFINITE_UCRT64_ROOT`。本项目使用滚动更新的MSYS2软件包，Preset固定构建参数但不锁定依赖版本。

不使用Preset时，在MSYS2 UCRT64终端使用以下手动命令：

```bash
cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_CXX_COMPILER=/ucrt64/bin/g++.exe -DCMAKE_PREFIX_PATH=/ucrt64 -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-ucrt64 --parallel 1
```

构建结果位于 `runtime/examples/` 下，每个示例都有独立的可执行文件、着色器、纹理和所需DLL。
只修改或新增示例的Shader、图片后，重新构建也会同步对应资源，不需要重新链接exe。这是构建时同步，不是运行中的热重载；删除源码资源不会自动删除运行目录中留下的旧副本。

运行测试：

```bash
ctest --test-dir build-ucrt64 --output-on-failure
```

每个示例都支持 `--smoke-test`：它创建隐藏窗口、运行三帧并检查像素；功能示例验证可见内容，空白模板验证清屏颜色。CTest覆盖全部示例入口；用 `ctest --preset msys2-ucrt64-debug --show-only` 查询当前登记列表。

面剔除测试读取隐藏窗口的实际像素，覆盖正面可见、背面剔除、关闭/重新开启以及镜像缩放。
同时检查立方体12个三角形的外向绕序，并比较8个视角下启用/禁用剔除时的图像。

Alpha混合示例先画不透明背景，再按摄像机空间深度从远到近画透明物体；透明阶段保留深度测试并关闭深度写入，结束后恢复状态。当前按物体中心排序，适用于互不穿插的示例物体；相交透明网格需要更细的排序策略。

运行 `runtime/examples/alpha_blending/alpha_blending.exe` 可看到蓝灰底板上重叠的暖色、冷色半透明圆形。
右侧浅色窄条是不透明遮挡物，圆形后绘制时也不能覆盖它。圆形RGBA图片由示例生成，不需要外部贴图文件。
本例使用非预乘Alpha，纹理Alpha与材质Alpha相乘；未引入sRGB颜色管理。

框架接口为 `setAlphaBlendingEnabled(bool)` 和 `setDepthWriteEnabled(bool)`，不自动排序或切换其他状态。
`clear()`会临时打开深度写入以清除深度缓冲，然后恢复原开关。
排序辅助代码位于 `src/graphics/rendering/RenderItem.h/.cpp`，由Scene渲染和测试共用；透明示例现在也统一通过Scene绘制，不再自行维护排序与状态切换。

混合测试使用隐藏窗口和RGBA8离屏缓冲，检查Alpha为0/0.5/1、累积Alpha、纹理Alpha、乱序提交、
相反摄像机视角、墙后遮挡、连续帧深度清除，以及混合/深度写入开关恢复。

图片由 `ImageLoader` 使用 stb_image 解码，当前纹理示例仍使用简单的P3格式PPM图片，示例资源位于 `examples/textured_quad/assets/checker.ppm`。PNG、JPG、BMP和TGA也可以使用同一套加载接口。

## 日志与错误检查

通用日志位于 `src/core/Log.h`，不依赖OpenGL上下文。包含头文件后即可使用：

```cpp
#include "core/Log.h"

LOG_INFO("资源加载完成");
LOG_WARN("日志文件不可用，继续使用控制台");
LOG_ERROR("资源加载失败");

// 默认只显示Info及以上级别；需要查看调试信息时再降低阈值。
Log::setLevel(Log::Level::Debug);
LOG_DEBUG("用于排查问题的额外信息");
```

每条记录包含本地时间、级别、调用处文件名和行号，例如：

```text
[2026-10-03 22:00:00] [INFO] [main.cpp:80] Starting alpha_blending example
```

- 级别从低到高为 `Debug / Info / Warning / Error`；`Off` 关闭所有输出。被过滤的宏不会计算消息表达式。
- 默认输出到控制台的stderr；`Log::setConsoleEnabled(false)`只关闭控制台，不影响文件。
- `Log::setFile(path)`创建缺失的父目录，并追加写入文件。失败返回false，原文件输出保持不变。
- `Log::closeFile()`只关闭文件输出；日志每次写入都会刷新，并通过互斥锁避免多个线程交错写入。
- 文件保留UTF-8文本；控制台若显示中文乱码，应检查终端是否使用UTF-8。

所有示例均在创建窗口前打开自己的日志，例如
`runtime/examples/alpha_blending/logs/alpha_blending.log`。
位置相对于exe，不依赖从哪个目录启动；记录启动、正常退出和捕获到的异常，日志目录已被Git忽略。
当前是同步追加日志，没有异步队列、日志轮转或大小上限，不建议逐帧输出正常运行信息。

异常与日志的职责不同：Shader、图片等模块无法完成操作时抛出异常，应用入口用
`LOG_ERROR(exception.what())`统一记录，再决定退出。Shader异常包含源文件路径和驱动的编译/链接详情，
避免在底层先打印一遍、顶层又重复打印。测试程序仍保留独立的通过/失败输出，方便CTest报告结果。

OpenGL诊断位于 `src/graphics/rendering/OpenGLDebug.h`：

```cpp
#include "graphics/rendering/OpenGLDebug.h"

// 在完成一组操作后检查；需要当前线程拥有有效的OpenGL上下文。
INFINITE_GL_CHECK("资源初始化");
// ...完成一帧绘制...
INFINITE_GL_CHECK("本帧绘制");
```

它使用OpenGL 3.3已有的 `glGetError`，不要求4.3或调试回调扩展。所有示例在初始化和交换缓冲前调用。
定义 `NDEBUG` 的构建（如CMake Release）会移除宏检查；需要强制检查时，可以显式调用
`OpenGLDebug::checkErrors("检查区间", __FILE__, __LINE__)`，返回true表示没有错误，false表示发现错误或缺少上下文。
即使日志关闭，该函数仍返回真实检查结果。

注意：检查会读取消费所有待处理错误，不会抛异常、修复错误或自动退出；记录的源码位置是检查点，
不一定是出错的那条GL指令。排查时可以缩小两次检查之间的范围。
Shader编译/链接失败须检查各自状态，不会简单地表现为 `glGetError` 错误，两条诊断路径都保留。

CTest覆盖模块回归、glTF解析、内存纹理、Model缓存与实例化、漫反射及场景多光源、示例玩家控制器、内置几何体、存档和各示例入口冒烟。测试范围以CMake登记为准，避免文档中的固定数量落后于代码。
诊断测试使用隐藏窗口，验证真实驱动错误、错误消费、无上下文、日志过滤以及Shader异常详情。
故意写错的Shader仅放在 `tests/fixtures/shaders/`，不能作为正常渲染资源使用。
如果本机并行编译出现内存不足，可使用 `cmake --build build-ucrt64 --parallel 1` 降低并发。

## Application：推荐的新应用入口

从 `examples/application_template/main.cpp` 开始，它可以直接编译运行，只显示空白背景，不在框架里塞入三角形。完整用法见 `docs/应用运行层使用说明.md`。

Application负责事件、时间、应用更新、Scene更新、清屏、绘制及交换缓冲。功能示例只保留资源创建和运动逻辑；日志路径、Escape和三帧冒烟策略统一在 `examples/common/ExampleRun.h` 中。

- `initialize`：窗口上下文已创建，在这里准备Shader、Mesh、Material和场景物体。
- `onEvents`：事件收集后调用，暂停时也调用；处理退出等窗口级行为。
- `update`：在Scene更新前调用，适合应用级逻辑及增删物体。
- `afterRender`：绘制后、交换缓冲前调用，供像素读回等用途。

回调全部可选，不需要继承基类。物体的运动仍注册到 `GameObject::setUpdateCallback`；不要再在回调里调用pollEvents、Scene::update或swapBuffers，避免一帧执行两次。

最小化或帧缓冲尺寸为零时暂停更新/绘制，并等待事件（最长50毫秒，事件到达可提前唤醒）。普通失焦不暂停；隐藏冒烟窗口只要尺寸有效也正常绘制。初始化完成后开始计时，首帧和暂停恢复首帧运动dt为0，普通dt默认最多0.1秒。

`frameTime()`中的真实时间不截断，模拟时间只累计实际传给更新的dt；这不是固定步长物理模拟，卡顿丢掉的模拟时间不补跑。窗口拖动导致事件处理长时间阻塞时，也会应用dt上限。

Application仅能run一次。正常或异常退出都会清空Scene，再由成员析构关闭Window。GPU资源推荐在initialize里用shared_ptr交给场景和材质持有；借用局部GPU变量会在initialize返回后悬空。不要把共享资源保存到比Application更长寿的全局变量中。

## GameObject与Scene

`src/scene/`负责“场景里有哪些物体”，`Renderer::drawItems`负责“这些物体按什么顺序和状态绘制”。
以下是保留的低层手动用法，便于理解模块关系，假设window、mesh、material、renderer、camera、time已经创建。使用Application时不要再复制这段主循环：

```cpp
#include "scene/Scene.h"

// 借用模式下，Scene必须比它借用的资源更晚创建、更早销毁。
Scene scene;
auto &object = scene.createObject("角色");
object.setRenderable(mesh, material);
object.transform.position.x = 0.5f;
const ObjectId objectId = object.id();

// 回调保存应用自己的运动逻辑；Scene只决定更新顺序和active规则。
object.setUpdateCallback([](GameObject &self, float deltaTime)
{
    self.transform.rotateEuler({0.0f, deltaTime, 0.0f});
});

window.pollEvents();
time.update();
scene.update(time.deltaTime());
renderer.clear(0.03f, 0.04f, 0.07f, 1.0f);
scene.render(renderer, camera, window.aspectRatio());

// 长期保存ID而非引用，用之前重新查找；nullptr表示物体已不存在。
if (auto *found = scene.findObject(objectId))
{
    found->setActive(false);
}
scene.removeObject(objectId);
// 此后object引用已失效，不能再访问它。
```

生命周期与管理规则：

- Scene使用 `unique_ptr` 独占GameObject，新增对象时已有对象地址不变；删除其他对象也不会使它失效。
- ID从1开始、0无效，删除和clear后都不复用；ID只在原Scene中有意义，不是跨场景的全局标识。
- 名称可以重复。Scene和GameObject都禁止复制/移动，避免身份和借用地址混乱。
- `setRenderable(mesh, material)`是借用模式：GameObject借用Mesh和Material，Material继续借用Shader和Texture。删除物体不会删除共享GPU资源。
- `setRenderable(sharedMesh, sharedMaterial)`是推荐的跨作用域模式：GameObject持有Mesh和Material，Material持有Shader和Texture的 `shared_ptr`。它只管理C++对象引用，不能让OpenGL资源在Window销毁后继续析构。
- GameObject内部的 `Renderable` 组件负责Mesh、Material和透明排序参考点；GameObject仍负责身份、Transform、启用状态和更新回调。组件拆分用于隔离职责，不是完整ECS，也不会复制Mesh数据。
- 先销毁Scene，再销毁材质和GPU资源，最后关闭Window；被引用的GPU资源不可move走或提前释放。
- `setRenderable`一起绑定网格和材质，`clearRenderable`解除借用或共享引用；最后一份共享引用解除时资源随之释放。空物体、禁用物体不参与绘制。
- `Scene::update`按创建顺序调用active对象的回调。更新期间不能创建、删除、清空对象或递归update；回调可以替换或清空自己，下帧生效。
- 仅支持主线程操作；绘制期间不能增删物体、修改资源或销毁场景。Transform已经支持父子层级，ResourceManager支持Shader、Texture、Mesh、Material和Model缓存。SceneSerializer支持OBJ与.material文件引用自动恢复；glTF实例暂不支持存档，尝试保存时会明确报错。
- `SceneSerializer`使用版本4的可读文本格式，保存名称、启用状态、局部Transform、父子索引、透明排序参考点、Mesh/Material相对路径，以及内置几何体的生成参数与默认颜色。不会保存更新回调；旧版本1/2/3仍可读取。
- 保存先写同目录独占临时文件，检查刷新/关闭后再替换旧档，失败不主动截断或删除旧文件；不保证断电持久化、文件元数据保留或并发写入互斥。
- `Scene::createPrimitive`及`createCylinder`等便捷入口支持六种内置形状，自动准备网格和材质；详见[基础几何体使用说明](docs/基础几何体使用说明.md)。
- `ActionMap`位于输入模块，把多个键盘键或鼠标按钮映射为一个动作名称，支持持续、按下和释放查询。它使用OR语义，不自行轮询窗口；Application提供的映射通过 `application.actions()` 配置，再把 `application.input()` 传给查询函数。

渲染规则：

- Material默认 `RenderMode::Opaque`、`CullMode::None`；透明表面必须显式设置 `RenderMode::AlphaBlend`，闭合立方体可选 `CullMode::Back`。
- 先画不透明组（深度测试/写入开启、混合关闭），再按视空间Z从远到近稳定绘制透明组（深度测试保留、写入关闭）。等深度保持提交顺序。
- `GameObject::setSortOrigin`可指定局部空间参考点；此点经过物体变换和摄像机变换再排序，不只比较世界Z。相交透明网格仍需更细粒度策略。
- 每个物体应用自己的剔除方式；Material可开启镜像绕序修正，按最终世界变换处理自身或父节点的负缩放。glTF和内置几何体默认材质已开启；手动材质须显式选择。
- 绘制后恢复进入时的深度、混合、剔除状态及程序、VAO、活动纹理单元和0号纹理绑定；不恢复Shader的uniform内容。
- Scene不清屏、不切换帧缓冲或视口、不交换窗口缓冲。旧的单物体 `Renderer::draw`仍使用调用者手动配置的状态，不自动应用材质的渲染模式。
- 绘制列表只是统一组织多次draw，不是合并draw call或实例化渲染优化。

## Mesh、Material、Input与Camera

新网格推荐使用 `Vertex` 和可选的 `std::uint32_t` 索引：`Mesh(vertices, indices)` 自动创建EBO并使用 `glDrawElements`。旧的8个float布局入口仍兼容。`Mesh::bounds()`可取得绑定姿态边界；Vertex包含法线（location 3）、切线（4）和颜色Alpha（5），蒙皮以独立缓冲提供关节索引（6）和权重（7）。没有通用动态网格编辑接口。OBJ可以先解析成CPU侧的MeshData，再交给Mesh上传。

Material由Shader、基础颜色和可选Texture组成，不包含Mesh。材质接口约定 `baseColor`、`hasTexture` 和 `textureSampler`：没有纹理时Material会解绑0号纹理单元，并设置 `hasTexture = false`，避免采样上一个物体遗留的纹理。自定义Shader可以像scene_objects示例一样处理此开关，也可以使用不采样纹理的纯颜色Shader；未使用的uniform会忽略。框架提供内置几何体使用的通用Shader；具体展示效果的Shader仍属于示例层。带透明材质须显式使用 `RenderMode::AlphaBlend`。

Window当前只支持一个活动窗口。手动管理循环时，每帧先调用一次 `window.pollEvents()`；使用Application时由框架代为调用，再通过 `Input` 读取状态：`isKeyDown` 是持续按住，`wasKeyPressed` / `wasKeyReleased` 是本帧边沿；鼠标提供按键、位置、移动增量和滚轮增量。旧的 `isKeyPressed` 仍保留，等同于 `isKeyDown`。
同帧快速按下又松开时两个边沿都会保留；边沿和增量在下次pollEvents时清空，失焦会释放按住状态。Window还提供实际尺寸、帧缓冲尺寸、焦点、最小化状态、本帧事件及VSync开关；VSync保存的是应用请求，最终行为由驱动决定。

Camera默认是透视相机，也可以显式调用 `setView(position, target, up)`、`setPerspective(fov, near, far)` 或 `setOrthographic(height, near, far)`。参数会在设置时校验，投影矩阵每帧根据Window实际帧缓冲比例生成。

## 持续集成

工作流配置为：GitHub Actions在Windows的MSYS2 UCRT64环境中配置并编译全部目标、按cpu标签运行无窗口测试；另一个Linux任务在Xvfb和Mesa软件OpenGL下运行完整像素回归及示例冒烟测试。软件渲染回归不能替代本机Windows驱动检查。
可用 `ctest --preset msys2-ucrt64-debug -L cpu` 筛选无窗口测试，`-L graphics` 筛选真实OpenGL测试；附加 `--show-only` 只列出测试，不执行。工作流尚未推送执行，不能视为远端CI已经通过。

运行 `runtime/examples/scene_objects/scene_objects.exe` 可以看到共享网格的两个旋转立方体、
两张半透明色板、不透明背景和前景遮挡条。WASD移动摄像机，按住鼠标右键拖动观察，Space/Ctrl升降，Shift加速，R复位；方向键移动暖色立方体，Escape退出。
几何、棋盘图片生成和动画都留在示例层；日志位于示例自己的 `logs/scene_objects.log`。

`infinite_scene_test`无需窗口，验证ID、增删查找、地址稳定性、独立Transform和排序。
`infinite_scene_render_test`使用隐藏窗口和离屏缓冲读回像素，验证透明顺序、遮挡、禁用/解绑、
共享资源、混用剔除方式、删除后的资源复用及OpenGL状态恢复。

## 操作

- `W`、`A`、`S`、`D`：在 `scene_objects` 中移动示例摄像机；其他可交互示例移动其示例物体
- `scene_objects` 右键拖动：改变摄像机观察方向；`Space`/`Ctrl`：上下移动；`Shift`：加速；`R`：恢复摄像机初始位置
- `alpha_blending`：静态展示透明叠加和遮挡关系
- `scene_objects`：自动旋转两个立方体，自由观察场景，方向键移动暖色立方体
- `material_showcase`：加载Avocado、Barramundi Fish和Lantern三个真实glTF模型；P暂停旋转，自由摄像机操作同上
- `Escape`：退出程序
