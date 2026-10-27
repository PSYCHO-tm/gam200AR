//
// Created by Zac Y on 22/10/2026.
//
#pragma once
#include "Types.hpp"
#include "ActionType.hpp"

static_assert(static_cast<unsigned>(ACTIONS::COUNT) <= 64, "Mask holds at most 64 actions");

using InputMask = UI32;
constexpr InputMask Bit(ACTIONS a) { return InputMask{1} << static_cast<unsigned>(a); }

// Shell sets this once at startup (and on DPI change). All distances are in pixels.
// e.g. tapRadiusPx = 10.0f * displayDensity on Android, so it is ~10 dp physically.
struct InputConfig
{
    F32 tapRadiusPx   = 20.0f;
    F32 tapMaxSeconds = 0.3f;
};

// Live input accumulator, written at event rate by the shell, drained once per frame.
struct InputState
{
    /** DISCRETE INPUT */
    InputMask held = 0;       // level
    InputMask pressed = 0;    // latched since last Poll
    InputMask released = 0;   // latched since last Poll

    bool Held(ACTIONS a) const     { return held & Bit(a); }
    bool Pressed(ACTIONS a) const  { return pressed & Bit(a); }
    bool Released(ACTIONS a) const { return released & Bit(a); }

    /** CONTINUOUS INPUT */
    F32 px = 0, py = 0;   // pixels, origin top-left
    F32 dx = 0, dy = 0;   // pixels, accumulated since last Poll
    F32 pressX = 0, pressY = 0;       // pixels, position at the moment of the latest press
    bool ptrDown = false, ptrPressed = false, ptrReleased = false, ptrCancelled = false;
};

// Frozen, read-only view for the whole frame. Rebuilt in full by ProcessInput, nothing else writes it.
struct InputSnapshot
{
    InputState is;
    F32 tapX = 0, tapY = 0; // pixel pos if tapped
    F32 heldTime = 0;
    bool tapped = false; // gesture output: a tap completed this frame
};

class InputProcessing
{
    InputState state{};
    InputSnapshot snapshot{};
    InputConfig config{};
    F32 heldTime_ = 0; // persists across frames (gesture tracking)

    // Single-consumer: clears edges/deltas. Only ProcessInput may call it.
    InputState Poll();

public:
    void SetConfig(InputConfig const& c);
    void SetAction(ACTIONS a, bool isHeld);
    void SetPointer(F32 nx, F32 ny, bool down);

    void Reset();
    void CancelPointer();

    // Core-facing: once per frame. Returns state, then clears edges/deltas.

    //InputState const& getState() const;
    InputSnapshot const& getSnapshot() const;

    void ProcessInput(F32 dt);
};

//KOPITWIN_INPUT_H
