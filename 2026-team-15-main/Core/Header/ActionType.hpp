#pragma once
#include <cstdint>

enum class ACTIONS : std::uint8_t
{
    EMPTY, PLAYSOUND, PAUSESOUND, STOPSOUND, LOOP, COUNT,
};// keep Count last