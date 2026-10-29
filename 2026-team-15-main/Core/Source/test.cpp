//
// Created by Zac Y on 4/10/2026.
//
#include "../Header/test.hpp"

namespace TestCore
{
    std::string OnTouch()
    {
        InputSnapshot const& snapshot = Core::GetInput().GetSnapshot();
        return "Taps: " + std::to_string(snapshot.FrameTapCount) +
               " Coords: " + std::to_string(snapshot.Is.Px) + ", " + std::to_string(snapshot.Is.Py) +
               " Held: " + std::to_string(snapshot.FrameHeldTime);
    }
}