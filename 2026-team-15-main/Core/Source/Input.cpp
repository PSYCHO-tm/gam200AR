//
// Created by Zac Y on 22/10/2026.
//
#include <cmath>
#include "Input.hpp"

void InputProcessing::SetConfig(InputConfig const& c) { config = c; }

void InputProcessing::SetAction(ACTIONS a, bool isHeld)
{
    const InputMask b = Bit(a);
    const bool prevHeld = (state.held & b) != 0;
    if (isHeld == prevHeld)
        return;

    if (isHeld)
    {
        state.held |= b;
        state.pressed  |= b;
    }
    else
    {
        state.held &= ~b;
        state.released |= b;
    }
}

void InputProcessing::SetPointer(F32 x, F32 y, bool down)
{
    if (down && !state.ptrDown)
    {
        state.ptrPressed = true;           // jump to touch point, no delta
        state.pressX = x, state.pressY = y;
    }
    else if(state.ptrDown)
    {
        state.dx += x - state.px;
        state.dy += y - state.py;
    }

    if (!down && state.ptrDown)
        state.ptrReleased = true;

    state.px = x; state.py = y; state.ptrDown = down;
}

// Focus loss / Android onPause: release everything so nothing sticks
// A pointer that was down is cancelled, not released, so it can never become a tap.
void InputProcessing::Reset()
{
    state.released |= state.held;
    state.held = state.pressed = 0;
    CancelPointer();
}

void InputProcessing::CancelPointer()
{
    if (state.ptrDown)
        state.ptrCancelled = true;
    state.ptrDown = state.ptrPressed = false;
    state.dx = state.dy = 0;
}

// Private: Returns state, then clears edges/deltas.
InputState InputProcessing::Poll()
{
    InputState out = state;
    state.pressed = state.released = 0;
    state.ptrPressed = state.ptrReleased = state.ptrCancelled = false;
    state.dx = state.dy = 0;
    return out;
}

InputSnapshot const& InputProcessing::getSnapshot() const { return snapshot; }

void InputProcessing::ProcessInput(F32 dt)
{
    const InputState is = Poll();

    // Gesture tracking (persists across frames)
    if (is.ptrPressed)   heldTime_ = 0;
    if (is.ptrDown || is.ptrReleased) heldTime_ += dt;   // include the release frame
    if (is.ptrCancelled) heldTime_ = 0;

    // Rebuild the snapshot in full: one writer, frozen for the rest of the frame.
    snapshot = InputSnapshot{};
    snapshot.is = is;
    snapshot.heldTime = heldTime_;

    /// Tap: genuinely released soon after pressing, without moving far (all in pixels)
    if (is.ptrReleased && !is.ptrCancelled)
    {
        const float mx = is.px - snapshot.tapX;
        const float my = is.py - snapshot.tapX;
        const bool shortPress = snapshot.heldTime < 0.3f;
        const bool smallMove  = std::sqrt(mx * mx + my * my) < config.tapRadiusPx;   // pixels
        if (shortPress && smallMove)
        {
            snapshot.tapped = true;
            snapshot.tapX = is.px;
            snapshot.tapY = is.py;
        }
    }
}