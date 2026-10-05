#include "TestSupport.h"
#include "core/Application.h"
#include "graphics/resources/Material.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Shader.h"

#include <GL/glew.h>
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
    ApplicationConfig testConfig()
    {
        ApplicationConfig config;
        config.width = 160;
        config.height = 160;
        config.visible = false;
        config.title = "Application Test";
        config.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        return config;
    }

    // 自定义删除器只记录证据，不在析构路径抛异常。
    // 即使生命周期回归出错，也先记录失败，避免在无上下文时继续调用GL析构造成崩溃。
    struct Lifetime
    {
        bool meshReleased = false;
        bool shaderReleased = false;
        bool contextAlive = true;
    };

    GameObject &createObject(Application &app, Lifetime &lifetime)
    {
        const auto *context = glfwGetCurrentContext();
        const std::vector<Vertex> vertices = {
            {{-0.8f, -0.8f, 0}, {1, 1, 1}, {0, 0}},
            {{ 0.8f, -0.8f, 0}, {1, 1, 1}, {1, 0}},
            {{ 0.0f,  0.8f, 0}, {1, 1, 1}, {0.5f, 1}}
        };
        auto mesh = std::shared_ptr<Mesh>(new Mesh(vertices), [&lifetime, context](Mesh *value)
        {
            lifetime.meshReleased = true;
            const bool current = glfwGetCurrentContext() == context;
            lifetime.contextAlive = lifetime.contextAlive && current;
            if (current)
            {
                delete value;
            }
        });
        auto shader = std::shared_ptr<Shader>(
            new Shader("tests/fixtures/culling.vert", "tests/fixtures/culling.frag"),
            [&lifetime, context](Shader *value)
        {
            lifetime.shaderReleased = true;
            const bool current = glfwGetCurrentContext() == context;
            lifetime.contextAlive = lifetime.contextAlive && current;
            if (current)
            {
                delete value;
            }
        });
        auto material = std::make_shared<Material>(shader, glm::vec4(1, 0, 0, 1));
        auto &object = app.scene().createObject("owned triangle");
        object.setRenderable(mesh, material);
        return object;
    }

    void requireReleased(const Lifetime &lifetime)
    {
        require(lifetime.meshReleased && lifetime.shaderReleased && lifetime.contextAlive,
            "Resources did not release while their context was current");
    }

    void normalRun()
    {
        Lifetime lifetime;
        {
            Application app(testConfig());
            app.window().setVSyncEnabled(false);
            std::vector<std::string> order;
            int renders = 0;
            ApplicationCallbacks callbacks;
            callbacks.initialize = [&](Application &application)
            {
                require(application.frameTime().realElapsedTime == 0.0, "Clock started before initialization");
                expectThrow<std::logic_error>([&] { application.run(); }, "Recursive run accepted");
                auto &object = createObject(application, lifetime);
                object.setUpdateCallback([&](GameObject &, float)
                {
                    order.push_back("object");
                });
            };
            callbacks.onEvents = [&](Application &)
            {
                order.push_back("events");
                // 通过已注册的GLFW回调馈入输入，验证Application确实在更新前收集/刷新输入。
                auto *handle = glfwGetCurrentContext();
                const auto key = glfwSetKeyCallback(handle, nullptr);
                glfwSetKeyCallback(handle, key);
                key(handle, GLFW_KEY_W, 0, renders == 0 ? GLFW_PRESS : GLFW_RELEASE, 0);
            };
            callbacks.update = [&](Application &application, float deltaTime)
            {
                order.push_back("update");
                require(deltaTime >= 0.0f && deltaTime <= 0.1f, "Unbounded simulation delta");
                require(application.scene().objectCount() == 1, "Initialization object was lost");
                require(!lifetime.meshReleased, "Initialization resources expired");
                if (renders == 0)
                {
                    require(deltaTime == 0.0f, "First update includes startup time");
                    require(application.input().wasKeyPressed(Key::W), "Press not delivered before update");
                }
                else
                {
                    require(application.input().wasKeyReleased(Key::W) &&
                        !application.input().wasKeyPressed(Key::W), "Input edges were not reset");
                }
            };
            callbacks.afterRender = [&](Application &application)
            {
                order.push_back("render");
                const auto size = application.window().framebufferSize();
                std::array<unsigned char, 4> pixel{};
                glReadPixels(size.x / 2, size.y / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
                require(pixel[0] > 240 && pixel[1] < 5, "Application did not draw Scene");
                if (++renders == 2)
                {
                    application.requestClose();
                }
            };
            app.run(callbacks);
            require(order == std::vector<std::string>{
                "events", "update", "object", "render", "events", "update", "object", "render"},
                "Application stage order changed");
            require(app.scene().objectCount() == 0, "Normal exit did not clear Scene");
            requireReleased(lifetime);
            require(glGetError() == GL_NO_ERROR, "Normal Application generated a GL error");
            expectThrow<std::logic_error>([&] { app.run(); }, "Application ran twice");
        }
        require(glfwGetCurrentContext() == nullptr, "Application did not destroy Window");
    }

    enum class Stage { Initialize, Events, Update, Object, Render };

    void exitsAndFailures()
    {
        // 每个阶段分别验证主动关闭和抛异常；异常消息必须保留原样。
        for (bool fail : {false, true})
        {
            for (Stage stage : {Stage::Initialize, Stage::Events, Stage::Update, Stage::Object, Stage::Render})
            {
                Lifetime lifetime;
                Application app(testConfig());
                std::vector<Stage> entered;
                const auto enter = [&](Application &application, Stage current)
                {
                    entered.push_back(current);
                    if (stage == current)
                    {
                        if (fail)
                        {
                            throw std::runtime_error("original application failure");
                        }
                        application.requestClose();
                    }
                };
                ApplicationCallbacks callbacks;
                callbacks.initialize = [&](Application &application)
                {
                    auto &object = createObject(application, lifetime);
                    object.setUpdateCallback([&](GameObject &, float)
                    {
                        enter(app, Stage::Object);
                    });
                    enter(application, Stage::Initialize);
                };
                callbacks.onEvents = [&](Application &application) { enter(application, Stage::Events); };
                callbacks.update = [&](Application &application, float) { enter(application, Stage::Update); };
                callbacks.afterRender = [&](Application &application) { enter(application, Stage::Render); };
                bool thrown = false;
                try
                {
                    app.run(callbacks);
                }
                catch (const std::runtime_error &error)
                {
                    thrown = true;
                    require(std::string(error.what()) == "original application failure", "Exception was replaced");
                }
                require(thrown == fail, "Failure was swallowed or close threw");
                require(entered.back() == stage &&
                    entered.size() == static_cast<std::size_t>(stage) + 1, "Later stage executed after exit");
                require(app.scene().objectCount() == 0, "Exit did not clear Scene");
                requireReleased(lifetime);
                expectThrow<std::logic_error>([&] { app.run(); }, "Finished/failed Application restarted");
            }
        }
    }

    void pauseAndResize()
    {
        Application app(testConfig());
        app.window().setVSyncEnabled(false);
        int events = 0;
        int updates = 0;
        int renders = 0;
        ApplicationCallbacks callbacks;
        callbacks.onEvents = [&](Application &application)
        {
            ++events;
            auto *handle = glfwGetCurrentContext();
            const auto iconify = glfwSetWindowIconifyCallback(handle, nullptr);
            glfwSetWindowIconifyCallback(handle, iconify);
            // 自动化不操作用户桌面：直接驱动窗口回调，验证暂停分支与Application接线。
            // 第1轮绘制，第2/3轮暂停，第4轮恢复，第5轮关闭。
            if (events == 2)
            {
                iconify(handle, GLFW_TRUE);
                require(application.window().isMinimized(), "Iconify state not recorded");
            }
            if (events == 4)
            {
                iconify(handle, GLFW_FALSE);
                glfwSetWindowSize(handle, 200, 180);
            }
            if (events == 5)
            {
                application.requestClose();
            }
            require(events <= 5, "Pause loop failed to exit");
        };
        callbacks.update = [&](Application &, float deltaTime)
        {
            require(events == 1 || events == 4, "Paused application was updated");
            require(deltaTime == 0.0f, "First/resume frame moved simulation");
            ++updates;
        };
        callbacks.afterRender = [&](Application &application)
        {
            require(events == 1 || events == 4, "Paused application rendered");
            const auto size = application.window().framebufferSize();
            // 真实尺寸查询与窗口回调共同用于渲染；不是把固定1280/800写死在循环里。
            require(size.x > 0 && size.y > 0, "Invalid framebuffer after resize");
            ++renders;
        };
        app.run(callbacks);
        require(events == 5 && updates == 2 && renders == 2, "Pause/resume loop count wrong");
        require(app.frameTime().simulationTime == 0.0, "Paused time entered simulation");
    }

    void constructionAndUnusedCleanup()
    {
        auto config = testConfig();
        config.maximumDeltaTime = 0.0f;
        expectThrow<std::invalid_argument>([&] { Application invalid(config); }, "Invalid max delta accepted");
        require(glfwGetCurrentContext() == nullptr, "Invalid config created a Window");
        config = testConfig();
        config.clearColor.x = std::numeric_limits<float>::quiet_NaN();
        expectThrow<std::invalid_argument>([&] { Application invalid(config); }, "NaN clear color accepted");

        Lifetime lifetime;
        {
            Application app(testConfig());
            createObject(app, lifetime);
            // 即使用户还没调用run就离开作用域，成员声明顺序也必须保证Scene先于Window释放。
        }
        requireReleased(lifetime);
        require(glfwGetCurrentContext() == nullptr, "Unused Application left context alive");
    }
}

int main()
{
    try
    {
        constructionAndUnusedCleanup();
        normalRun();
        exitsAndFailures();
        pauseAndResize();
        std::cout << "Application lifecycle, early exit, input and pause passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
