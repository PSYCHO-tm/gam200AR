/* ======================================================================================
Main Author
Name:           Zachary Yee
Course:         IMGD
Digipen ID:     zachary.y
====================================================================================== */
#pragma once
#include <bitset>
#include "Types.hpp"
#include "ActionType.hpp"

static_assert(static_cast<unsigned>(ACTIONS::COUNT) <= 32, "Mask holds at most 64 actions");

using InputMask = UI32;
constexpr InputMask Bit(ACTIONS a) { return InputMask{1} << static_cast<unsigned>(a); }

using KeyId = US;
constexpr size_t kMaxKeys = 512;
struct KeyState
{
	std::bitset<kMaxKeys> Held, Pressed, Released;
};

// Shell sets this once at startup (and on DPI change). All distances are in pixels.
// e.g. tapRadiusPx = 10.0f * displayDensity on Android, so it is ~10 dp physically.
struct InputConfig
{
    F32 TapRadiusPx   = 20.0f;
    F32 TapMaxSeconds = 0.25f;
};

// Live input accumulator, written at event rate by the shell, drained once per frame.
struct InputState
{
    /** ACTION INPUT */
    InputMask Held = 0;       // level
    InputMask Pressed = 0;    // latched since last Poll
    InputMask Released = 0;   // latched since last Poll

    [[nodiscard]] bool IsHeld(ACTIONS a) const     { return Held & Bit(a); }
    [[nodiscard]] bool IsPressed(ACTIONS a) const  { return Pressed & Bit(a); }
    [[nodiscard]] bool IsReleased(ACTIONS a) const { return Released & Bit(a); }

    bool PtrDown = false, PtrPressed = false, PtrReleased = false, PtrCancelled = false;
	/** CONTINUOUS INPUT */
	F32      Px = 0, Py = 0;         // pixels, origin top-left
	F32      Dx = 0, Dy = 0;         // pixels, accumulated since last Poll
	F32      PressX = 0, PressY = 0; // pixels, position at the moment of the latest press

    /** KEY INPUT */
    KeyState Keys;
};

// Frozen, read-only view for the whole frame. Rebuilt in full by ProcessInput, nothing else writes it.
struct InputSnapshot
{
    InputState Is;
    F32 TapX = 0, TapY = 0; // pixel pos if tapped
    F32 FrameHeldTime = 0;
    UI32 FrameTapCount = 0; // gesture output: a tap completed this frame
    bool Tapped = false;
};

class InputProcessing
{
    InputState State{};
    InputSnapshot Snapshot{};
    InputConfig Config{};
    UI32 TapCount{};
    F32 HeldTime{}; // persists across frames (gesture tracking)

    // Single-consumer: clears edges/deltas. Only ProcessInput may call it.
    InputState Poll();

public:
    void SetConfig(InputConfig const& c);
    void SetAction(ACTIONS a, bool isHeld);
    void SetPointer(F32 nx, F32 ny, bool down);
	void SetKey(KeyId k, bool isDown);
	void CancelKey();
    void Reset();
    void CancelPointer();
	void CancelInMask();

    // Core-facing: once per frame. Returns state, then clears edges/deltas.

    //InputState const& getState() const;
    [[nodiscard]]InputSnapshot const& GetSnapshot() const;

    void ProcessInput(F32 dt);
};
//KOPITWIN_INPUT_H