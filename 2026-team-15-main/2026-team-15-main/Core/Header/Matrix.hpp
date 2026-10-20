#pragma once
#include "Vector.hpp"

// Convention: ROW vectors.  v' = v * M   (leftmost matrix applies first)
// Storage:    m[row][col], row-major.
// Translation lives in the BOTTOM row.
// Spaces: S, R, T are all local -> world pieces; SRT() gives local -> world.

/*------------------ Matrix3x3 (2D) -------------------*/
class Matrix3x3
{
public:
    float m[3][3];

public:
    Matrix3x3(); // Identity

    static Matrix3x3 Translate(float tx, float ty);
    static Matrix3x3 Scale(float sx, float sy);
    static Matrix3x3 Rotate(float degrees);
    static Matrix3x3 SRT(Vector2D const& scale, float degrees, Vector2D const& position);

    Matrix3x3 Transpose() const;
    Vector2D TransformPoint(Vector2D const& p) const;     // w = 1
    Vector2D TransformDirection(Vector2D const& d) const; // w = 0
};
Matrix3x3 operator*(Matrix3x3 const& lhs, Matrix3x3 const& rhs);

/*------------------ Matrix4x4 (3D) -------------------*/
class Matrix4x4
{
public:
    float m[4][4];

public:
    Matrix4x4(); // Identity

    static Matrix4x4 Translate(float tx, float ty, float tz);
    static Matrix4x4 Scale(float sx, float sy, float sz);
    static Matrix4x4 RotateX(float degrees);
    static Matrix4x4 RotateY(float degrees);
    static Matrix4x4 RotateZ(float degrees);
    static Matrix4x4 Rotate(Vector3D const& eulerDegrees); // X, then Y, then Z
    static Matrix4x4 SRT(Vector3D const& scale, Vector3D const& eulerDegrees, Vector3D const& position);

    Matrix4x4 Transpose() const;
    Vector3D TransformPoint(Vector3D const& p) const;     // w = 1
    Vector3D TransformDirection(Vector3D const& d) const; // w = 0
};
Matrix4x4 operator*(Matrix4x4 const& lhs, Matrix4x4 const& rhs);
