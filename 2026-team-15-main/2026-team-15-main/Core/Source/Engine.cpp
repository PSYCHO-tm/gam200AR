//
// Created by Zac Y on 15/10/2026.
//

#include <algorithm>

#include "test.hpp"
#include "Engine.hpp"
#include "Time/Time.hpp"
#include "Log/Profiler.hpp"

namespace
{
    constexpr int maxSteps{5};

    void FixedUpdate(F32 fixedDt) { /* physics, collision */ }
    void Update(F32 dt)           { /* camera, particles, animation, ECS systems */ }
}

void Core::Init()
{
    Timer::Init();
    Profiler::Init();
}

void Core::Frame(UI64 nowT)
{
    Profiler::BeginFrame();

    /** Update time variables */
    Timer::Tick(nowT);
    unscaledDt = std::min(static_cast<F32>(Timer::GetFrameTime()), maxDt);
    dt = timeScale * unscaledDt;
    frameCount = Timer::GetFrameCount();
    frameRate = static_cast<F32>(Timer::GetFrameRate());

    /** Check accumulator against fixedDt to ensure FixedUpdate happens 0-5 times depending on clamped dt. */
    accumulator += dt;
    for (int steps = 0; steps < maxSteps && accumulator >= fixedDt; ++steps)
    {
        PROFILE_SCOPE("FixedUpdate");
        FixedUpdate(fixedDt);
        accumulator -= fixedDt;
    }
    // Hit maxSteps with time still owed: zero accum.
    if (accumulator >= fixedDt)
        accumulator = 0.f;

    // Per Frame.
    PROFILE_SCOPE("Update");
    Update(dt);

    Profiler::EndFrame();
}

void Core::OnResume()
{
    Timer::Reset();
    //accumulator = 0.f;    // also forget any time owed from before the pause
}

UI32 Core::GetOverlayText(char* buf, UI32 size)
{
    UI32 n = std::snprintf(buf, size,
                           "frame %u, dt = %.2f ms, fps = %.1f",
                           frameCount, dt * 1000.0f, frameRate);
    if (n < size)
        n += Profiler::Format(buf + n, size - n);

    return n;
}

void Core::PauseTime()
{
    isPaused = !isPaused;
    timeScale = isPaused ? 0.0f : 1.0f;
}