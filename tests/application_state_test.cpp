#include "TestSupport.h"
#include "core/Application.h"
#include "game/state/ApplicationStateStack.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    struct Trace
    {
        std::vector<std::string> events;
        int sceneUpdates = 0;
    };

    class SecondState;
    class ThirdState;

    class FirstState final : public ApplicationState
    {
    public:
        explicit FirstState(Trace &trace) : trace_(trace) {}

        void onEnter(ApplicationStateContext &context) override
        {
            trace_.events.push_back("enter:first");
            auto &object = context.application().scene().createObject("state test object");
            object.script().setUpdateCallback([this](GameObject &, float)
            {
                ++trace_.sceneUpdates;
            });
        }

        void onPause(ApplicationStateContext &) override { trace_.events.push_back("pause:first"); }
        void onExit(ApplicationStateContext &) override { trace_.events.push_back("exit:first"); }
        void onEvents(ApplicationStateContext &context) override;

    private:
        Trace &trace_;
        int eventCount_ = 0;
    };

    class SecondState final : public ApplicationState
    {
    public:
        explicit SecondState(Trace &trace) : trace_(trace) {}

        void onEnter(ApplicationStateContext &) override { trace_.events.push_back("enter:second"); }
        void onExit(ApplicationStateContext &) override { trace_.events.push_back("exit:second"); }
        void onEvents(ApplicationStateContext &context) override;

        StateFramePolicy framePolicy() const noexcept override
        {
            // 验证暂停/覆盖状态可以继续接收输入，但不推进Scene和物理。
            StateFramePolicy policy;
            policy.updateScene = false;
            policy.simulatePhysics = false;
            return policy;
        }

    private:
        Trace &trace_;
        int eventCount_ = 0;
    };

    class ThirdState final : public ApplicationState
    {
    public:
        explicit ThirdState(Trace &trace) : trace_(trace) {}

        void onEnter(ApplicationStateContext &) override { trace_.events.push_back("enter:third"); }
        void onExit(ApplicationStateContext &) override { trace_.events.push_back("exit:third"); }
        void onEvents(ApplicationStateContext &context) override
        {
            if (eventCount_++ == 0)
            {
                context.quit();
            }
        }

    private:
        Trace &trace_;
        int eventCount_ = 0;
    };

    void FirstState::onEvents(ApplicationStateContext &context)
    {
        if (eventCount_++ == 0)
        {
            context.push(std::make_unique<SecondState>(trace_));
        }
    }

    void SecondState::onEvents(ApplicationStateContext &context)
    {
        if (eventCount_++ == 0)
        {
            context.replace(std::make_unique<ThirdState>(trace_));
        }
    }
}

int main()
{
    try
    {
        Trace trace;
        ApplicationConfig config;
        config.width = 160;
        config.height = 160;
        config.visible = false;
        config.title = "Application State Test";
        config.audio.enabled = false;

        Application application(config);
        ApplicationStateStack states;
        states.push<FirstState>(trace);
        application.run(states);

        require(trace.events == std::vector<std::string>{
            "enter:first", "pause:first", "enter:second", "exit:second",
            "enter:third", "exit:third", "exit:first"},
            "Application state lifecycle order is incorrect");
        require(trace.sceneUpdates == 2,
            "State frame policy did not pause Scene updates for the overlay state");
        require(states.empty(), "State stack was not cleared during application shutdown");
        std::cout << "Application state stack passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
