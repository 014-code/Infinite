# 最小UI使用说明

`src/ui/` 是当前引擎的第一阶段UI层。它的目标是支持主菜单、暂停菜单和结算界面所需的基础能力，而不是马上成为完整的编辑器或网页排版系统。

## 模块职责

```text
UiCanvas
├── UiPanel       背景面板和子元素坐标系
├── UiLabel       轻量文字
└── UiButton      鼠标命中、键盘焦点和激活回调
```

- `UiElement`：可见性、启用状态、局部矩形和父子层级；不调用OpenGL。
- `UiCanvas`：拥有根元素，执行命中测试、按钮状态和Tab焦点循环。
- `UiRenderer`：把绘制命令批量上传到一个动态顶点缓冲，在Scene之后绘制。
- `UiPanel`、`UiLabel`、`UiButton`：只描述控件本身，不知道Application或Scene。

UI布局使用窗口逻辑像素，左上角是 `(0, 0)`。Application会把它换算到实际帧缓冲尺寸，因此高DPI窗口不需要把布局数字乘两次。

## 创建一个面板和按钮

```cpp
#include "ui/UiButton.h"
#include "ui/UiLabel.h"
#include "ui/UiPanel.h"

auto &canvas = application.ui();
auto &panel = canvas.create<UiPanel>();
panel.setRect({360.0f, 180.0f, 560.0f, 420.0f});

auto &title = panel.createChild<UiLabel>("INFINITE");
title.setRect({20.0f, 24.0f, 520.0f, 48.0f});
title.setScale(4.0f);
title.setAlignment(UiTextAlign::Center);

auto &button = panel.createChild<UiButton>("PLAY");
button.setRect({160.0f, 150.0f, 240.0f, 64.0f});
button.setOnActivated([&application]
{
    // 下一阶段可以在这里请求GameStateStack切换。
    application.requestClose();
});
```

控件的位置是相对父元素的局部坐标。上面按钮的实际位置是面板左上角加上 `(160, 150)`；后添加的同级控件在命中测试中位于更上层。

## 输入和生命周期

Application每帧在应用事件回调后自动调用 `UiCanvas::processInput`，然后在Scene绘制完成后调用 `UiRenderer`。空Canvas不会创建UI Shader和GPU缓冲。

- 鼠标左键按下时记录当前按钮，并设置Pressed状态。
- 鼠标左键在同一个按钮上释放时激活它；拖到其他控件上释放不会误触发。
- `Tab` 在可聚焦控件之间循环；按住Shift时反向循环。
- `Enter` 或 `Space` 激活当前焦点按钮。
- 按钮回调可以请求应用状态切换；Canvas自身对回调期间清空/重建元素有版本保护。

按钮回调不应保存按钮引用到异步任务中。清空Canvas或销毁状态后，原控件引用立即失效。

## 文字范围

标签只保存文本和布局属性。默认使用内置5×7位图字形，覆盖英文大写字母、数字和常用菜单标点；
小写英文会自动转成大写，旧示例不需要额外字体文件。

如果应用配置了TTF/OTF字体，Renderer会启用UTF-8字体路径，支持中文/英文混排、基础换行和左中右对齐：

```cpp
ApplicationConfig config;
config.uiFontPath = "assets/fonts/NotoSansSC-Regular.otf";
Application application(config);
```

字体资源由应用提供，不硬编码Windows系统字体路径。当前还没有接入HarfBuzz，
因此阿拉伯文、印度语连字和复杂双向文字暂不保证正确显示。

## 当前边界

目前没有：

- 图片控件、滚动容器、文本输入和复选框；
- 自动布局、锚点编辑器和响应式约束；
- HarfBuzz级别的复杂文字排版、字体变体和Emoji彩色字形；
- UI动画时间线和主题资源文件；
- 独立UI渲染通道或批处理缓存。

第3项的主菜单、暂停菜单和结果界面会基于这些基础控件实现，菜单状态本身仍由 `ApplicationStateStack` 管理。

## 验证

```powershell
E:/msys64/ucrt64/bin/cmake.exe --build build-final-debug --parallel 4
E:/msys64/ucrt64/bin/ctest.exe --test-dir build-final-debug --output-on-failure
```

`tests/ui_test.cpp`验证树形布局、鼠标命中、Pressed状态、鼠标激活、Tab焦点和Enter激活；
`tests/ui_font_test.cpp`验证UTF-8解码；`ui_demo`示例的冒烟测试验证真实OpenGL绘制。
