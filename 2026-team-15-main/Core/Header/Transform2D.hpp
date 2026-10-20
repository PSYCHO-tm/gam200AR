#pragma once
#include "Vector.hpp"
#include "Matrix.hpp"

// Plain data component for the ECS (2D). Rotation is a single angle because
// 2D can only rotate around the screen axis.
class Transform2D
{
    // Public Variables (order matches SRT)
    public:
        Vector2D scale{ 1.0f, 1.0f };
        float rotation = 0.0f; // Degrees
        Vector2D position{ 0.0f, 0.0f };

    // Constructors, Destructors
    public:
        Transform2D() = default;
        Transform2D(Vector2D const& scale, float rotation, Vector2D const& position);
        ~Transform2D() = default;

    // Public Functions
    public:
        Matrix3x3 GetMatrix() const; // local space -> world space (v * S * R * T)
};
