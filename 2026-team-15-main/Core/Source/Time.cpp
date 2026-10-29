/* ======================================================================================
Main Author
Name:           Zachary Yee
Course:         IMGD
Digipen ID:     zachary.y
====================================================================================== */
#include "Time.hpp"

namespace
{
    bool hasLast{}; // Bool for dt reset().
    UI64  lastT{}; // previous frame dt.
    F64  frameTime{}; // current dt. (Delta Time)
    UI32  frameCount{}; // Lifetime no. of frames since init
    F64  fps{}; // FPS taken by taking gaccumFrames/gaccum.
    F64  fpsWindowTime{}; // Time accumulated since last window.
    UI32  fpsWindowFrames{}; // Frames accumulated since last window.
}

void Timer::Init()
{
    Reset();
    frameCount = fpsWindowFrames = 0;
    fps = fpsWindowTime = 0.0;
}

void Timer::Reset()
{
    hasLast = false;
    frameTime = 0.0;
}

/** Get/Update the raw dt/frame time (nowT - lastT) & current fps. (accum/accumFrames) */
void Timer::Tick(UI64 nowT)
{
    if (hasLast)
    {
        frameTime = static_cast<F64>(nowT - lastT) * 1e-9;   // convert to seconds
        ++frameCount;

        fpsWindowTime += frameTime;
        ++fpsWindowFrames;
        /** Limit fps data to last 0.5 sec, so stats won't keep updating & it's as accurate as it can be.*/
        if (fpsWindowTime >= 0.5)
        {
            fps = fpsWindowFrames / fpsWindowTime;
            fpsWindowTime = 0.0;
            fpsWindowFrames = 0;
        }
    }
    lastT = nowT;
    hasLast = true;
}

F64 Timer::GetFrameTime() { return frameTime; }
UI32 Timer::GetFrameCount() { return frameCount; }
F64 Timer::GetFrameRate() { return fps; }