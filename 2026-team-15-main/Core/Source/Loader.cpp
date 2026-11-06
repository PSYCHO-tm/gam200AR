#include "Loader.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

#include "Geometry.hpp"
#include "Matrix.hpp"
#include "Renderer.hpp"

Loader& Loader::Get()
{
    static Loader instance;
    return instance;
}

void Loader::Draw(float seconds, int viewportW, int viewportH)
{
    if (!Registered)
    {
        DotMesh = Renderer::Get().AddMesh(MakeQuad()); // a 1 x 1 square from (0,0) to (1,1), facing the screen
        Registered = true;
    }

    // Straight onto the screen: view and projection are identity, so each model matrix places a dot in screen units
    // (-1..1 across). x is squeezed by the screen's shape so the ring stays round.
    float const aspect = (viewportW > 0 && viewportH > 0) ? static_cast<float>(viewportW) / viewportH : 1.0f;
    float const dotSize = 0.022f; // in screen-height units
    float const radius = 0.07f;
    int const dots = 10;
    float const stepsPerSec = 10.0f; // the bright dot moves one place every 0.1 s: one turn a second
    float const head = std::fmod(seconds * stepsPerSec, static_cast<float>(dots));
    float const degToRad = 3.14159265f / 180.0f;

    std::vector<Renderer::MeshDraw> draws;
    draws.reserve(dots);
    for (int i = 0; i < dots; ++i)
    {
        float const angle = 90.0f - 360.0f * i / dots; // clockwise from the top
        float const age =
            std::fmod(head - i + dots, static_cast<float>(dots)) / dots; // 0 = the bright one, then fading
        Renderer::MeshDraw dot;
        dot.MeshIndex = DotMesh;
        dot.Model =
            Matrix4x4::Translate(-0.5f, -0.5f, 0.0f) * // centre the square on its origin
            Matrix4x4::Scale(dotSize, dotSize, 1.0f) * // dot-sized
            Matrix4x4::Translate(radius * std::cos(angle * degToRad), radius * std::sin(angle * degToRad), 0.0f) *
            Matrix4x4::Scale(1.0f / aspect, 1.0f, 1.0f); // undo the screen's stretch
        dot.Tint = Vector3D{1.0f, 1.0f, 1.0f};           // tint 1 = plain white, no lighting
        dot.TintAmount = 1.0f;
        dot.Alpha = 0.15f + 0.7f * (1.0f - age);
        dot.Occludable = false; // a screen item: never hidden by the depth picture
        draws.push_back(dot);
    }
    Renderer::Get().DrawMeshes(draws, Matrix4x4{}, Matrix4x4{});
}
