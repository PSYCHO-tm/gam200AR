//
// Created by Zac Y on 4/10/2026.
//
#include "../Header/test.hpp"

namespace TestCore
{
    std::string OnTouch()
    {
        InputSnapshot const& snapshot = Core::GetInput().getSnapshot();
        //InputState const& state = Core::GetInput().getState();
        return "Tapped: " + std::to_string(snapshot.tapped) +
               " X Coords: " + std::to_string(snapshot.is.px) +
               " Y Coords: " + std::to_string(snapshot.is.py) +
               " Held Time: " + std::to_string(snapshot.heldTime);
    }
}