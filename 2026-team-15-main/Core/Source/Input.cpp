/* ======================================================================================
Main Author
Name:           Zachary Yee
Course:         IMGD
Digipen ID:     zachary.y
====================================================================================== */
#include "Input.hpp"
#include <cmath>

void InputProcessing::SetConfig(InputConfig const& c) { Config = c; }

void InputProcessing::SetAction(ACTIONS a, bool isHeld)
{
    const InputMask b = Bit(a);
    const bool prevHeld = (State.Held & b) != 0;
    if (isHeld == prevHeld) return;

    if (isHeld)
    {
        State.Held |= b;
        State.Pressed |= b;
    }
    else
    {
        State.Held &= ~b;
        State.Released |= b;
    }
}

void InputProcessing::SetPointer(F32 x, F32 y, bool down)
{
    if (down && !State.PtrDown)
    {
        State.PtrPressed = true; // jump to touch point, no delta
        State.PressX = x, State.PressY = y;
    }
    else if (State.PtrDown)
    {
        State.Dx += x - State.Px;
        State.Dy += y - State.Py;
    }

    if (!down && State.PtrDown) State.PtrReleased = true;

    State.Px = x;
    State.Py = y;
    State.PtrDown = down;
}

void InputProcessing::SetKey(KeyId k, bool isDown)
{
    if (k >= kMaxKeys || State.Keys.Held.test(k) == isDown) return; // same dedupe as SetAction
    State.Keys.Held.set(k, isDown);
    (isDown ? State.Keys.Pressed : State.Keys.Released).set(k);
}

// Focus loss / Android onPause: release everything so nothing sticks
// A pointer that was down is cancelled, not released, so it can never become a tap.
void InputProcessing::Reset()
{
    CancelInMask();
    CancelPointer();
    CancelKey();
}

void InputProcessing::CancelPointer()
{
    if (State.PtrDown) State.PtrCancelled = true;
    State.PtrDown = State.PtrPressed = false;
    State.Dx = State.Dy = 0;
}

void InputProcessing::CancelKey()
{
    State.Keys.Released |= State.Keys.Held;
    State.Keys.Held.reset();
    State.Keys.Pressed.reset();
}

void InputProcessing::CancelInMask()
{
    State.Released |= State.Held;
    State.Held = State.Pressed = 0;
}
// Private: Returns state, then clears edges/deltas.

InputState InputProcessing::Poll()
{
    InputState out = State;
    State.Pressed = State.Released = 0;
    State.PtrPressed = State.PtrReleased = State.PtrCancelled = false;
    State.Keys.Pressed.reset(), State.Keys.Released.reset();
    State.Dx = State.Dy = 0;
    return out;
}

InputSnapshot const& InputProcessing::GetSnapshot() const { return Snapshot; }

void InputProcessing::ProcessInput(F32 dt)
{
    const InputState is = Poll();

    // Gesture tracking (persists across frames)
    if (is.PtrPressed) HeldTime = 0;
    if (is.PtrDown || is.PtrReleased) HeldTime += dt; // include the release frame
    if (is.PtrCancelled) HeldTime = 0;

    // Rebuild the snapshot in full: one writer, frozen for the rest of the frame.
    Snapshot = InputSnapshot{};
    Snapshot.Is = is;
    Snapshot.FrameHeldTime = HeldTime;
    Snapshot.FrameTapCount = TapCount;

    /// Tap: genuinely released soon after pressing, without moving far (all in pixels)
    if (is.PtrReleased && !is.PtrCancelled)
    {
        const float mx = is.Px - is.PressX;
        const float my = is.Py - is.PressY;
        const bool shortPress = Snapshot.FrameHeldTime < Config.TapMaxSeconds;
        const bool smallMove = std::sqrt(mx * mx + my * my) < Config.TapRadiusPx; // pixels

        if (shortPress && smallMove)
        {
            Snapshot.Tapped = true, Snapshot.FrameTapCount = ++TapCount;
            Snapshot.TapX = is.Px;
            Snapshot.TapY = is.Py;
        }
    }
}