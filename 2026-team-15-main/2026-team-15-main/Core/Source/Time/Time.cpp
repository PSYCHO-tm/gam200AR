//
// Created by Zac Y on 15/10/2026.
//
#include "Time/Time.hpp"

namespace
{
    bool ghasLast{}; // Bool for dt reset().
    UI64  glastT{}; // previous frame dt.
    F64  gframeTime{}; // current dt. (Delta Time)
    UI32  gframeCount{}; // Lifetime no. of frames since init
    F64  gfps{}; // FPS taken by taking gaccumFrames/gaccum.
    F64  gFpsWindowTime{}; // Time accumulated since last window.
    UI32  gFpsWindowFrames{}; // Frames accumulated since last window.
}

void Timer::Init()
{
    Reset();
    gframeCount = gFpsWindowFrames = 0;
    gfps = gFpsWindowTime = 0.0;
}

void Timer::Reset()
{
    ghasLast = false;
    gframeTime = 0.0;
}

/** Get/Update the raw dt/frame time (nowT - lastT) & current fps. (accum/accumFrames) */
void Timer::Tick(UI64 nowT)
{
    if (ghasLast)
    {
        gframeTime = static_cast<F64>(nowT - glastT) * 1e-9;   // convert to seconds
        ++gframeCount;

        gFpsWindowTime += gframeTime;
        ++gFpsWindowFrames;
        /** Limit fps data to last 0.5 sec, so stats won't keep updating & it's as accurate as it can be.*/
        if (gFpsWindowTime >= 0.5)
        {
            gfps = gFpsWindowFrames / gFpsWindowTime;
            gFpsWindowTime = 0.0;
            gFpsWindowFrames = 0;
        }
    }
    glastT = nowT;
    ghasLast = true;
}

F64 Timer::GetFrameTime() { return gframeTime; }
UI32 Timer::GetFrameCount() { return gframeCount; }
F64 Timer::GetFrameRate() { return gfps; }