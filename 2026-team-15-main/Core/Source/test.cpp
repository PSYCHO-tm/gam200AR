//
// Created by Zac Y on 4/10/2026.
//
#include "../Header/test.hpp"

namespace TestCore
{
    std::string HelloWorld() { return std::string{"Hello World C++\n"}; }

    std::string OnTouch(int action, float x, float y)
    {
        return "core got action = " + std::to_string(action) +
               " x= " + std::to_string(x) + " y= " + std::to_string(y);
    }

    /*void CTX::Update(double dt, const Input& in) {
        prevAngle = currAngle;                      // snapshot for interpolation
        currAngle += static_cast<float>((1.0 + in.spin) * dt);
    }
    void CTX::Render(double alpha) {
        float a = prevAngle + (currAngle - prevAngle) * static_cast<float>(alpha);
        // draw the triangle rotated by `a`
    }*/
}