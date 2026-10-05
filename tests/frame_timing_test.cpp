#include "TestSupport.h"
#include "core/FrameTiming.h"

#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    try
    {
        FrameTiming timing(0.1f);
        timing.advance(0.25f, false);
        require(timing.frame().realDeltaTime == 0.25f, "Real delta was changed");
        require(timing.frame().deltaTime == 0.0f, "First frame was not restarted");
        require(timing.frame().simulationTime == 0.0, "First frame advanced simulation");

        timing.advance(0.25f, false);
        require(timing.frame().deltaTime == 0.1f, "Large frame delta was not capped");
        require(timing.frame().realElapsedTime == 0.5, "Real elapsed time was not accumulated");
        timing.advance(0.4f, true);
        require(timing.frame().deltaTime == 0.0f && std::abs(timing.frame().simulationTime - 0.1) < 0.000001,
            "Paused frame advanced simulation");
        timing.advance(0.3f, true);
        require(timing.frame().deltaTime == 0.0f, "Continuous pause advanced simulation");
        timing.advance(0.02f, false);
        require(timing.frame().deltaTime == 0.0f, "Resume frame was not restarted");
        timing.advance(0.02f, false);
        require(timing.frame().deltaTime == 0.02f && std::abs(timing.frame().simulationTime - 0.12) < 0.000001,
            "Post-resume timing is wrong");

        expectThrow<std::invalid_argument>([] { FrameTiming invalid(0.0f); }, "Invalid cap accepted");
        expectThrow<std::invalid_argument>([&] { timing.advance(-1.0f, false); }, "Negative delta accepted");
        expectThrow<std::invalid_argument>([&] {
            timing.advance(std::numeric_limits<float>::quiet_NaN(), false);
        }, "NaN delta accepted");
        require(std::isfinite(timing.frame().realElapsedTime), "Timing became non-finite");
        std::cout << "Frame timing passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
