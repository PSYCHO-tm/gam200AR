//
// Created by Zac Y on 4/10/2026.
//
#include "../Header/test.hpp"

namespace TestCore
{
    std::string OnTouch(int action, float x, float y)
    {
        return "core got action = " + std::to_string(action) +
               " x= " + std::to_string(x) + " y= " + std::to_string(y);
    }
}