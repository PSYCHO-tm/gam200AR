//
// Created by Zac Y on 15/10/2026.
//
#pragma once

#include "Types.hpp"

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

    void Init();
    void Frame(UI64 nowT);   // Execute one iteration of the loop body
    void OnResume();         // Execute on e.g. if app returned from background

    UI32 GetOverlayText(char* buf, UI32 size);
    void PauseTime();
}