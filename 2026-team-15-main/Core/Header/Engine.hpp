//
// Created by Zac Y on 15/10/2026.
//
#pragma once

#include "Types.hpp"
#include "Input.hpp"

namespace Core
{
    constexpr F32 maxDt    = 0.1f;
    constexpr F32 fixedDt{ 1.f / 60.f };
    inline F32 unscaledDt{};
    inline F32 dt{};
    inline F32 accumulator{};

    inline F32 timeScale{ 1.f };
    inline F32 frameRate{};
    inline UI32 frameCount{};

    inline bool isPaused{ false };
    inline bool applicationRunning{ true };

    inline InputProcessing mInput;
    inline InputProcessing& GetInput() { return mInput; }

    inline int viewportW{ 1 };   // screen size in pixels, set by Resize
    inline int viewportH{ 1 };

    void Init();
    void Resize(int width, int height);   // when the surface size changes
    void Update(UI64 nowT);   // Execute one iteration of the loop body
    void Render();            // Draw: camera picture, then the equipment
    void Exit();
    void OnResume();         // Execute on e.g. if app returned from background

    UI32 GetOverlayText(char* buf, UI32 size);
    void PauseTime(bool p);
}