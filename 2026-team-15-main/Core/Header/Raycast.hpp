#pragma once
#include "Matrix.hpp"
#include "Vector.hpp"

// A half-line in WORLD space: starts at 'origin' and goes along 'direction' (unit length).
struct Ray
{
    Vector3D origin{0.0f, 0.0f, 0.0f};
    Vector3D direction{0.0f, 0.0f, -1.0f};
};

// Turns a touch into a world-space ray. The pixel is in screen space: (0,0) is the TOP-left, y grows downward.
// Use the same view and projection the renderer draws with, so a touch hits exactly what the player sees.
// Row vectors: a world point p reaches the screen as  p * view * projection.
Ray ScreenPointToRay(float pixelX, float pixelY, int viewportW, int viewportH, Matrix4x4 const& view,
                     Matrix4x4 const& projection);
