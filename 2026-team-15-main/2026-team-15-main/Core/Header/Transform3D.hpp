#pragma once
#include "Vector.hpp"
#include "Matrix.hpp"

// Plain data component for the ECS (3D). Position, rotation and scale stay as
// separate Vector3D values so the inspector can show and edit each one.
// The matrix is derived from them, never stored.
class Transform3D
{
    // Public Variables (order matches SRT)
    public:
        Vector3D scale{ 1.0f, 1.0f, 1.0f };
        Vector3D rotation{ 0.0f, 0.0f, 0.0f }; // Euler angles in degrees
        Vector3D position{ 0.0f, 0.0f, 0.0f };

    // Constructors, Destructors
    public:
        Transform3D() = default;
        Transform3D(Vector3D const& scale, Vector3D const& rotation, Vector3D const& position);
        ~Transform3D() = default;

    // Public Functions
    public:
        Matrix4x4 GetMatrix() const; // local space -> world space (v * S * R * T)
};
