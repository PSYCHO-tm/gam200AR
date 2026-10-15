//
// Created by Zac Y on 4/10/2026.
//
#ifndef TEST_CORE
#define TEST_CORE

#include <string>

namespace TestCore
{
    /*constexpr double kFixedDt = 1.0 / 60.0;

    struct Input { float spin = 0.f; bool tapped = false; };

    class CTX {
    public:
        void Update(double dt, const Input& in);   // advances state by exactly dt
        void Render(double alpha);                 // alpha in [0,1): draw prev..curr blend
    private:
        float prevAngle = 0.f, currAngle = 0.f;
    };*/

    std::string HelloWorld();

    std::string OnTouch(int action, float x, float y);
}
#endif