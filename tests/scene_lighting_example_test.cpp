#include "TestSupport.h"
#include <GL/glew.h>
#include "core/Application.h"
#include "scene_lighting/SceneLightingScene.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include <stb/stb_image_write.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

int main(int argc, char **argv)
{
    try
    {
        require(argc == 2, "Expected screenshot directory");
        const auto directory = std::filesystem::u8path(argv[1]);
        std::filesystem::create_directories(directory);
        ApplicationConfig config;
        config.visible = false;
        config.clearColor = {0.02f, 0.025f, 0.04f, 1.0f};
        Application app(config);
        require(&app.directionalLight() == &app.scene().lighting().mainLight(), "Application light is not the Scene light");
        ApplicationCallbacks callbacks;
        callbacks.initialize = [](Application &app) { SceneLightingExample::createScene(app); };
        callbacks.afterRender = [&](Application &app)
        {
            const auto size = app.window().framebufferSize();
            const auto bytes = static_cast<std::size_t>(size.x) * size.y * 4;
            std::vector<unsigned char> lit(bytes), baseline(bytes), topDown(bytes);
            glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, lit.data());

            // 相同布局关闭局部灯再画一遍，确认截图中的灯光确实产生可见贡献。
            auto original = app.scene().lighting();
            app.scene().lighting().pointLights.clear();
            app.scene().lighting().spotLights.clear();
            Renderer renderer;
            renderer.clear(.02f, .025f, .04f, 1);
            app.scene().render(renderer, app.camera(), app.window().aspectRatio());
            glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, baseline.data());
            app.scene().lighting() = original;
            std::size_t changed = 0;
            for (std::size_t i = 0; i < bytes; i += 4)
            {
                if (std::abs(int(lit[i]) - int(baseline[i])) > 2 ||
                    std::abs(int(lit[i + 1]) - int(baseline[i + 1])) > 2 ||
                    std::abs(int(lit[i + 2]) - int(baseline[i + 2])) > 2) { ++changed; }
            }
            require(changed > 500, "Local lights did not visibly affect the example");
            const auto rowBytes = static_cast<std::size_t>(size.x) * 4;
            for (int y = 0; y < size.y; ++y)
            {
                std::copy_n(lit.data() + (size.y - 1 - y) * rowBytes, rowBytes, topDown.data() + y * rowBytes);
            }
            require(stbi_write_png((directory / "scene-lighting.png").u8string().c_str(),
                size.x, size.y, 4, topDown.data(), size.x * 4) != 0, "Could not write scene lighting screenshot");
            require(glGetError() == GL_NO_ERROR, "Example left GL errors");
            app.requestClose();
        };
        app.run(callbacks);
        std::cout << "Real scene lighting example checked and captured\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
