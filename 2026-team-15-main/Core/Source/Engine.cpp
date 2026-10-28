//
// Created by Zac Y on 15/10/2026.
//

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include <GLES3/gl3.h>

#include "test.hpp"
#include "ARcore.hpp"
#include "Engine.hpp"
#include "EquipmentScene.hpp"
#include "Renderer.hpp"
#include "Time.hpp"
#include "Profiler.hpp"

namespace
{
    constexpr int maxSteps{5};

    // The camera of the last frame that was drawn. Update picks against it, so a tap hits what is on screen.
    bool      gTracking = false;
    Matrix4x4 gView;
    Matrix4x4 gProjection;

    // ARCore (and OpenGL) store 16 floats column-major for column vectors. That is exactly the memory layout of a
    // row-vector Matrix4x4, so the conversion is a plain copy.
    Matrix4x4 FromGL(float const* values)
    {
        Matrix4x4 matrix;
        std::memcpy(matrix.m, values, sizeof(matrix.m));
        return matrix;
    }

    void FixedUpdate(F32 fixedDt) { /* physics, collision */ }
    void UpdateSystems(F32 const& dt)
    {
        Core::mInput.ProcessInput(dt);
        EquipmentScene::Get().Update(dt, Core::mInput.getSnapshot(), gTracking, gView, gProjection, Core::viewportW,
                                     Core::viewportH);
        /* camera, particles, animation, ECS systems */
    }
}

void Core::Init()
{
    Timer::Init();
    Profiler::Init();
    ArCameraInit();                  // camera texture + camera program (this is a new GL context)
    Renderer::Get().Init();          // forgets old GPU handles, then builds the lit shader
    EquipmentScene::Get().Init();
}

void Core::Resize(int width, int height)
{
    glViewport(0, 0, width, height);
    viewportW = width > 0 ? width : 1;
    viewportH = height > 0 ? height : 1;
}

void Core::Update(UI64 nowT)
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
    // Per Frame.
    PROFILE_SCOPE("Update");
    UpdateSystems(dt);

    for (int steps = 0; steps < maxSteps && accumulator >= fixedDt; ++steps)
    {
        PROFILE_SCOPE("FixedUpdate");
        FixedUpdate(fixedDt);
        accumulator -= fixedDt;
    }
    // Hit maxSteps with time still owed: zero accum.
    if (accumulator >= fixedDt)
        accumulator = 0.f;

    Profiler::EndFrame();
}

void Core::Render()
{
    // 1. The camera picture. This also reads ARCore's camera for this frame.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ArCameraDraw();

    // 2. The equipment, only while ARCore is tracking (the same rule the sphere had).
    float view[16];
    float projection[16];
    gTracking = ArGetCameraMatrices(view, projection);
    if (!gTracking)
    {
        EquipmentScene::Get().SetMarker(0, Matrix4x4{});
        EquipmentScene::Get().SetPlanes({}, {});
        return;
    }
    gView       = FromGL(view);
    gProjection = FromGL(projection);

    // The pieces follow the anchor: ARCore keeps correcting its pose, and the scene re-places them on it.
    float anchorMatrix[16];
    if (ArGetAnchorPose(anchorMatrix))
    {
        EquipmentScene::Get().SetAnchorPose(FromGL(anchorMatrix));
    }

    // The small debug square: where the middle of the screen touches a plane (and, once placed, where the anchor is).
    float markerMatrix[16] = {};
    int   markerState      = ArGetPlacementMarker(markerMatrix);
    EquipmentScene::Get().SetMarker(markerState, FromGL(markerMatrix));

    // The planes ARCore is tracking, each drawn as a see-through rectangle.
    constexpr int maxPlanes = 8;
    float         planeMatrices[maxPlanes * 16] = {};
    float         planeExtents[maxPlanes * 2]   = {};
    int const     planeCount                    = ArGetPlanes(planeMatrices, planeExtents, maxPlanes);
    std::vector<Matrix4x4> planePoses;
    std::vector<Vector2D>  planeSizes;
    for (int i = 0; i < planeCount; ++i)
    {
        planePoses.push_back(FromGL(planeMatrices + i * 16));
        planeSizes.push_back(Vector2D{planeExtents[i * 2], planeExtents[i * 2 + 1]});
    }
    EquipmentScene::Get().SetPlanes(planePoses, planeSizes);

    EquipmentScene::Get().Draw(gView, gProjection);
}

void Core::Exit() {}

void Core::OnResume()
{
    Timer::Reset();
    //accumulator = 0.f;    // also forget any time owed from before the pause
}

UI32 Core::GetOverlayText(char* buf, UI32 size)
{
    UI32 n = std::snprintf(buf, size,
                           "%s\nframe %u, dt = %.2f ms, fps = %.1f",
                           ArGetPlacementStatus(), frameCount, dt * 1000.0f, frameRate);
    if (n < size)
        n += Profiler::Format(buf + n, size - n);

    return n;
}

void Core::PauseTime(bool p)
{
    isPaused = p;
    timeScale = isPaused ? 0.0f : 1.0f;
}