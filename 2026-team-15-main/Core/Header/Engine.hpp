/* ======================================================================================
Main Author
Name:           Zachary Yee
Course:         IMGD
Digipen ID:     zachary.y
====================================================================================== */
#pragma once

#include "Types.hpp"
#include "Input.hpp"
#include "Coordinator.hpp"
#include "DefinitionLoader.hpp"
#include "JsonSceneCoordinator.hpp"
#include "RegisterSystem.hpp"
#include "Time.hpp"
#include "Profiler.hpp"
#include "Audio/audioSystem.hpp"
#include <memory>

namespace Ecs{ class Coordinator; }

namespace Core
{
    Ecs::Coordinator& GetWorld();

    constexpr F32 maxDt    = 0.1f;
    constexpr F32 fixedDt{ 1.f / 60.f };
    inline F32 unscaledDt{};
    inline F32 dt{};
    inline F32 accumulator{};

    inline F32 timeScale{ 1.f };
    inline F32 frameRate{};
    inline UI32 frameCount{};

    inline bool isPaused{ false };
    // M2G05: true while the AR station is hidden (not placed yet, tracking lost, or the card covered). Game time (dt)
    // stops so a graded session is not spoiled, and starts again on its own once tracking is back. Separate from
    // isPaused (the player's Pause button), so neither one undoes the other.
    inline bool isTrackingHeld{ false };
    //inline bool applicationRunning{ true };

    inline InputProcessing mInput;
    inline InputProcessing& GetInput() { return mInput; }

    inline Ecs::Database db;
    inline std::unique_ptr<Ecs::JsonScene> jc;
    inline std::optional<Ecs::GameSystems> sys;

    bool TryInitialize(); // Moved implementation of Init() to here + added logging (Raphael's acceptance criteria)
    inline AudioSystem audio;

    bool Init();
    void Start();
    void Update(UI64 nowT = Timer::Now()); // Execute one iteration of the loop body
    void Render();            // Draw: camera picture, then the equipment
    void Exit();
    inline int viewportW{ 1 };   // screen size in pixels, set by Resize
    inline int viewportH{ 1 };

    void Resize(int width, int height, int rotation);   // when the surface size changes
    void OnResume();         // Execute on e.g. if app returned from background

    UI32 GetOverlayText(char* buf, UI32 size);
    UI32 GetActionsText(char* buf, UI32 size);
    void DoAction(char const* actionId);
    void PauseTime(bool p);
}