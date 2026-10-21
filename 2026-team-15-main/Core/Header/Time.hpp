//
// Created by Zac Y on 15/10/2026.
//
#pragma once
#include <chrono>
#include "Types.hpp"

namespace Timer
{
    inline UI64 Now()
    {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    }

    void Init();            // Start the timer.
    void Reset();           // Set next tick dt to 0
    void Tick(UI64 nowT);   // Call exactly once per frame
}

/** Read-only accessors */
namespace Timer
{
    F64 GetFrameTime();     // Get the current dt.
    UI32 GetFrameCount();   // Get lifetime no. of frames.
    F64 GetFrameRate();     // Get the current FPS.
}