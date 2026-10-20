#include "Matrix.hpp"
#include <cmath> // std::sin, std::cos

namespace
{
    const float gdeG2Rad = 3.14159265358979f / 180.0f;
}

/*------------------ Matrix3x3 -------------------*/
Matrix3x3::Matrix3x3()
{
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            this->m[r][c] = (r == c) ? 1.0f : 0.0f;
}

Matrix3x3 Matrix3x3::Translate(float tx, float ty)
{
    Matrix3x3 result;
    result.m[2][0] = tx;
    result.m[2][1] = ty;
    return result;
}

Matrix3x3 Matrix3x3::Scale(float sx, float sy)
{
    Matrix3x3 result;
    result.m[0][0] = sx;
    result.m[1][1] = sy;
    return result;
}

Matrix3x3 Matrix3x3::Rotate(float degrees)
{
    float rad = degrees * gdeG2Rad;
    float c = std::cos(rad);
    float s = std::sin(rad);

    Matrix3x3 result;
    result.m[0][0] = c;   result.m[0][1] = s;
    result.m[1][0] = -s;  result.m[1][1] = c;
    return result;
}

Matrix3x3 Matrix3x3::SRT(Vector2D const& scale, float degrees, Vector2D const& position)
{
    return Scale(scale.x, scale.y) * Rotate(degrees) * Translate(position.x, position.y);
}

Matrix3x3 Matrix3x3::Transpose() const
{
    Matrix3x3 result;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            result.m[r][c] = this->m[c][r];
    return result;
}

Vector2D Matrix3x3::TransformPoint(Vector2D const& p) const
{
    return Vector2D(
        p.x * this->m[0][0] + p.y * this->m[1][0] + this->m[2][0],
        p.x * this->m[0][1] + p.y * this->m[1][1] + this->m[2][1]);
}

Vector2D Matrix3x3::TransformDirection(Vector2D const& d) const
{
    return Vector2D(
        d.x * this->m[0][0] + d.y * this->m[1][0],
        d.x * this->m[0][1] + d.y * this->m[1][1]);
}

Matrix3x3 operator*(Matrix3x3 const& lhs, Matrix3x3 const& rhs)
{
    Matrix3x3 result;
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k)
                sum += lhs.m[r][k] * rhs.m[k][c];
            result.m[r][c] = sum;
        }
    }
    return result;
}

/*------------------ Matrix4x4 -------------------*/
Matrix4x4::Matrix4x4()
{
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            this->m[r][c] = (r == c) ? 1.0f : 0.0f;
}

Matrix4x4 Matrix4x4::Translate(float tx, float ty, float tz)
{
    Matrix4x4 result;
    result.m[3][0] = tx;
    result.m[3][1] = ty;
    result.m[3][2] = tz;
    return result;
}

Matrix4x4 Matrix4x4::Scale(float sx, float sy, float sz)
{
    Matrix4x4 result;
    result.m[0][0] = sx;
    result.m[1][1] = sy;
    result.m[2][2] = sz;
    return result;
}

Matrix4x4 Matrix4x4::RotateX(float degrees)
{
    float rad = degrees * gdeG2Rad;
    float c = std::cos(rad);
    float s = std::sin(rad);

    Matrix4x4 result;
    result.m[1][1] = c;   result.m[1][2] = s;
    result.m[2][1] = -s;  result.m[2][2] = c;
    return result;
}

Matrix4x4 Matrix4x4::RotateY(float degrees)
{
    float rad = degrees * gdeG2Rad;
    float c = std::cos(rad);
    float s = std::sin(rad);

    Matrix4x4 result;
    result.m[0][0] = c;  result.m[0][2] = -s;
    result.m[2][0] = s;  result.m[2][2] = c;
    return result;
}

Matrix4x4 Matrix4x4::RotateZ(float degrees)
{
    float rad = degrees * gdeG2Rad;
    float c = std::cos(rad);
    float s = std::sin(rad);

    Matrix4x4 result;
    result.m[0][0] = c;   result.m[0][1] = s;
    result.m[1][0] = -s;  result.m[1][1] = c;
    return result;
}

Matrix4x4 Matrix4x4::Rotate(Vector3D const& eulerDegrees)
{
    return RotateX(eulerDegrees.x) * RotateY(eulerDegrees.y) * RotateZ(eulerDegrees.z);
}

Matrix4x4 Matrix4x4::SRT(Vector3D const& scale, Vector3D const& eulerDegrees, Vector3D const& position)
{
    return Scale(scale.x, scale.y, scale.z)
         * Rotate(eulerDegrees)
         * Translate(position.x, position.y, position.z);
}

Matrix4x4 Matrix4x4::Transpose() const
{
    Matrix4x4 result;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            result.m[r][c] = this->m[c][r];
    return result;
}

Vector3D Matrix4x4::TransformPoint(Vector3D const& p) const
{
    return Vector3D(
        p.x * this->m[0][0] + p.y * this->m[1][0] + p.z * this->m[2][0] + this->m[3][0],
        p.x * this->m[0][1] + p.y * this->m[1][1] + p.z * this->m[2][1] + this->m[3][1],
        p.x * this->m[0][2] + p.y * this->m[1][2] + p.z * this->m[2][2] + this->m[3][2]);
}

Vector3D Matrix4x4::TransformDirection(Vector3D const& d) const
{
    return Vector3D(
        d.x * this->m[0][0] + d.y * this->m[1][0] + d.z * this->m[2][0],
        d.x * this->m[0][1] + d.y * this->m[1][1] + d.z * this->m[2][1],
        d.x * this->m[0][2] + d.y * this->m[1][2] + d.z * this->m[2][2]);
}

Matrix4x4 operator*(Matrix4x4 const& lhs, Matrix4x4 const& rhs)
{
    Matrix4x4 result;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
                sum += lhs.m[r][k] * rhs.m[k][c];
            result.m[r][c] = sum;
        }
    }
    return result;
}
