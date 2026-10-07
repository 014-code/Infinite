#include "../common/ExampleRun.h"

#include "ui/UiButton.h"
#include "ui/UiLabel.h"
#include "ui/UiPanel.h"

// 这个示例只展示UI层，不创建Scene物体。
// 它用于确认面板、标签、鼠标按钮和键盘焦点都能在Application上层复用。
int main(int argc, char *argv[])
{
    return runExample(argc, argv, "ui_demo", "Infinite - Minimal UI",
        {0.035f, 0.045f, 0.07f, 1.0f},
        [](Application &application, const std::filesystem::path &directory)
        {
            (void)directory;
            auto &canvas = application.ui();
            auto &panel = canvas.create<UiPanel>(glm::vec4(0.08f, 0.11f, 0.18f, 0.96f));
            panel.setRect({360.0f, 180.0f, 560.0f, 420.0f});

            auto &title = panel.createChild<UiLabel>("INFINITE UI");
            title.setRect({20.0f, 28.0f, 520.0f, 48.0f});
            title.setScale(4.0f);
            title.setAlignment(UiTextAlign::Center);

            auto &hint = panel.createChild<UiLabel>("MOUSE OR TAB + ENTER");
            hint.setRect({20.0f, 92.0f, 520.0f, 32.0f});
            hint.setScale(2.0f);
            hint.setColor({0.64f, 0.72f, 0.86f, 1.0f});
            hint.setAlignment(UiTextAlign::Center);

            auto &status = panel.createChild<UiLabel>("READY");
            status.setRect({20.0f, 310.0f, 520.0f, 32.0f});
            status.setScale(2.0f);
            status.setAlignment(UiTextAlign::Center);

            auto &button = panel.createChild<UiButton>("ACTIVATE");
            button.setRect({150.0f, 170.0f, 260.0f, 70.0f});
            button.setOnActivated([&status]
            {
                // 只改变UI自己的显示状态，真正的菜单切换将在下一阶段接入GameStateStack。
                status.setText("ACTIVATED");
            });
        }, ExampleFrameContent::DrawnScene);
}
