// Tests/test_math.cpp  --  Catch2 tests for Vector, Matrix and Transform
// Conventions (from your headers): ROW vectors, v' = v * M, translation in the bottom row,
// rotation turns counter-clockwise (right-handed), Euler angles apply X, then Y, then Z.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <limits>

#include "Vector.hpp"
#include "Matrix.hpp"
#include "Transform2D.hpp"
#include "Transform3D.hpp"

namespace
{
    // Floats are never exact, and Approx needs a margin to compare against 0.
    Catch::Approx Near(float v) { return Catch::Approx(v).margin(1e-4); }

    void CheckNear(Vector2D const& got, Vector2D const& want)
    {
        CAPTURE(got.x, got.y, want.x, want.y);
        CHECK(got.x == Near(want.x));
        CHECK(got.y == Near(want.y));
    }

    void CheckNear(Vector3D const& got, Vector3D const& want)
    {
        CAPTURE(got.x, got.y, got.z, want.x, want.y, want.z);
        CHECK(got.x == Near(want.x));
        CHECK(got.y == Near(want.y));
        CHECK(got.z == Near(want.z));
    }

    template <typename M, int N>
    void CheckMat(M const& got, M const& want)
    {
        for (int r = 0; r < N; ++r)
            for (int c = 0; c < N; ++c)
            {
                CAPTURE(r, c, got.m[r][c], want.m[r][c]);
                CHECK(got.m[r][c] == Near(want.m[r][c]));
            }
    }

    void CheckMat(Matrix3x3 const& got, Matrix3x3 const& want) { CheckMat<Matrix3x3, 3>(got, want); }
    void CheckMat(Matrix4x4 const& got, Matrix4x4 const& want) { CheckMat<Matrix4x4, 4>(got, want); }

    const float NaN = std::numeric_limits<float>::quiet_NaN();
}

// ===================================================================== Vector2D
TEST_CASE("Vector2D basics", "[vector]")
{
    CheckNear(Vector2D(), Vector2D(0.0f, 0.0f));
    CHECK(Vector2D(3.0f, 4.0f).Magnitude() == Near(5.0f));
    CheckNear(Vector2D(3.0f, 4.0f).Normalize(), Vector2D(0.6f, 0.8f));
    CHECK(Vector2D(3.0f, 4.0f).Normalize().Magnitude() == Near(1.0f));
    CHECK(Vector2D(1.0f, 2.0f).DotProduct(Vector2D(3.0f, 4.0f)) == Near(11.0f));
    CHECK(Vector2D(1.0f, 2.0f).CrossProduct(Vector2D(3.0f, 4.0f)) == Near(-2.0f));
}

TEST_CASE("Vector2D operators", "[vector]")
{
    const Vector2D a(1.0f, 2.0f), b(3.0f, 4.0f);
    CheckNear(a + b, Vector2D(4.0f, 6.0f));
    CheckNear(b - a, Vector2D(2.0f, 2.0f));
    CheckNear(a * b, Vector2D(3.0f, 8.0f));
    CheckNear(b / a, Vector2D(3.0f, 2.0f));

    Vector2D v = a; v += b;     CheckNear(v, Vector2D(4.0f, 6.0f));
    v = a;          v += 1.0f;  CheckNear(v, Vector2D(2.0f, 3.0f));
    v = b;          v -= a;     CheckNear(v, Vector2D(2.0f, 2.0f));
    v = b;          v -= 1.0f;  CheckNear(v, Vector2D(2.0f, 3.0f));
    v = a;          v *= b;     CheckNear(v, Vector2D(3.0f, 8.0f));
    v = a;          v *= 2.0f;  CheckNear(v, Vector2D(2.0f, 4.0f));
    v = b;          v /= a;     CheckNear(v, Vector2D(3.0f, 2.0f));
    v = b;          v /= 2.0f;  CheckNear(v, Vector2D(1.5f, 2.0f));

    CHECK((a == Vector2D(1.0f, 2.0f)));
    CHECK((a != b));
}

TEST_CASE("Vector2D edge cases", "[vector][edge]")
{
    CHECK(Vector2D(0.0f, 0.0f).Magnitude() == Near(0.0f));
    CheckNear(Vector2D(0.0f, 0.0f).Normalize(), Vector2D(0.0f, 0.0f));   // zero stays zero, no NaN
    CHECK(Vector2D(-3.0f, -4.0f).Magnitude() == Near(5.0f));
    CheckNear(Vector2D(-3.0f, -4.0f).Normalize(), Vector2D(-0.6f, -0.8f));

    const Vector2D a(2.0f, -3.0f), b(-5.0f, 4.0f);
    CheckNear(a.Normalize().Normalize(), a.Normalize());
    CHECK(Vector2D(1.0f, 0.0f).DotProduct(Vector2D(0.0f, 1.0f)) == Near(0.0f));
    CHECK(a.DotProduct(b) == Near(b.DotProduct(a)));
    CHECK(Vector2D(1.0f, 2.0f).CrossProduct(Vector2D(2.0f, 4.0f)) == Near(0.0f));   // parallel
    CHECK(a.CrossProduct(a) == Near(0.0f));
    CHECK(a.CrossProduct(b) == Near(-b.CrossProduct(a)));

    // the right-hand side is the same object as the left
    Vector2D s(3.0f, 4.0f); s += s;  CheckNear(s, Vector2D(6.0f, 8.0f));
    s = Vector2D(3.0f, 4.0f); s -= s;  CheckNear(s, Vector2D(0.0f, 0.0f));
    s = Vector2D(3.0f, 4.0f); s *= s;  CheckNear(s, Vector2D(9.0f, 16.0f));
    s = Vector2D(3.0f, 4.0f); s /= s;  CheckNear(s, Vector2D(1.0f, 1.0f));

    // plain float division follows IEEE: x/0 = +-inf
    const Vector2D dz = Vector2D(1.0f, -1.0f) / Vector2D(0.0f, 0.0f);
    CHECK((std::isinf(dz.x) && dz.x > 0.0f));
    CHECK((std::isinf(dz.y) && dz.y < 0.0f));

    CHECK((Vector2D(-0.0f, 0.0f) == Vector2D(0.0f, 0.0f)));          // -0 equals +0
    CHECK_FALSE((Vector2D(NaN, 0.0f) == Vector2D(NaN, 0.0f)));       // NaN is never equal
}

// ===================================================================== Vector3D
TEST_CASE("Vector3D basics", "[vector]")
{
    CheckNear(Vector3D(), Vector3D(0.0f, 0.0f, 0.0f));
    CHECK(Vector3D(1.0f, 2.0f, 2.0f).Magnitude() == Near(3.0f));
    CheckNear(Vector3D(0.0f, 0.0f, 5.0f).Normalize(), Vector3D(0.0f, 0.0f, 1.0f));
    CHECK(Vector3D(1.0f, 2.0f, 3.0f).DotProduct(Vector3D(4.0f, 5.0f, 6.0f)) == Near(32.0f));
    CheckNear(Vector3D(1.0f, 0.0f, 0.0f).CrossProduct(Vector3D(0.0f, 1.0f, 0.0f)), Vector3D(0.0f, 0.0f, 1.0f));
    CheckNear(Vector3D(0.0f, 1.0f, 0.0f).CrossProduct(Vector3D(1.0f, 0.0f, 0.0f)), Vector3D(0.0f, 0.0f, -1.0f));
}

TEST_CASE("Vector3D operators", "[vector]")
{
    const Vector3D a(1.0f, 2.0f, 3.0f), b(4.0f, 5.0f, 6.0f);
    CheckNear(a + b, Vector3D(5.0f, 7.0f, 9.0f));
    CheckNear(b - a, Vector3D(3.0f, 3.0f, 3.0f));
    CheckNear(a * b, Vector3D(4.0f, 10.0f, 18.0f));
    CheckNear(b / a, Vector3D(4.0f, 2.5f, 2.0f));

    Vector3D v = a; v += b;     CheckNear(v, Vector3D(5.0f, 7.0f, 9.0f));
    v = a;          v += 1.0f;  CheckNear(v, Vector3D(2.0f, 3.0f, 4.0f));
    v = b;          v -= a;     CheckNear(v, Vector3D(3.0f, 3.0f, 3.0f));
    v = b;          v -= 1.0f;  CheckNear(v, Vector3D(3.0f, 4.0f, 5.0f));
    v = a;          v *= b;     CheckNear(v, Vector3D(4.0f, 10.0f, 18.0f));
    v = a;          v *= 2.0f;  CheckNear(v, Vector3D(2.0f, 4.0f, 6.0f));
    v = b;          v /= a;     CheckNear(v, Vector3D(4.0f, 2.5f, 2.0f));
    v = b;          v /= 2.0f;  CheckNear(v, Vector3D(2.0f, 2.5f, 3.0f));

    CHECK((a == Vector3D(1.0f, 2.0f, 3.0f)));
    CHECK((a != b));
}

TEST_CASE("Vector3D edge cases", "[vector][edge]")
{
    CHECK(Vector3D(0.0f, 0.0f, 0.0f).Magnitude() == Near(0.0f));
    CheckNear(Vector3D(0.0f, 0.0f, 0.0f).Normalize(), Vector3D(0.0f, 0.0f, 0.0f));   // zero stays zero, no NaN
    CheckNear(Vector3D(-2.0f, 0.0f, 0.0f).Normalize(), Vector3D(-1.0f, 0.0f, 0.0f));

    const Vector3D v(3.0f, -4.0f, 12.0f);
    CHECK(v.Magnitude() == Near(13.0f));
    CheckNear(v.Normalize().Normalize(), v.Normalize());

    CheckNear(Vector3D(1.0f, 2.0f, 3.0f).CrossProduct(Vector3D(2.0f, 4.0f, 6.0f)), Vector3D(0.0f, 0.0f, 0.0f));
    const Vector3D p(1.0f, 2.0f, 3.0f), q(-4.0f, 5.0f, 0.5f);
    const Vector3D pq = p.CrossProduct(q);
    CHECK(pq.DotProduct(p) == Near(0.0f));   // cross result is perpendicular to both inputs
    CHECK(pq.DotProduct(q) == Near(0.0f));
    const Vector3D qp = q.CrossProduct(p);
    CheckNear(pq, Vector3D(-qp.x, -qp.y, -qp.z));

    Vector3D t(1.0f, 2.0f, 3.0f); t += t;  CheckNear(t, Vector3D(2.0f, 4.0f, 6.0f));
    t = Vector3D(1.0f, 2.0f, 3.0f); t -= t;  CheckNear(t, Vector3D(0.0f, 0.0f, 0.0f));
    t = Vector3D(1.0f, 2.0f, 3.0f); t *= t;  CheckNear(t, Vector3D(1.0f, 4.0f, 9.0f));
    t = Vector3D(1.0f, 2.0f, 3.0f); t /= t;  CheckNear(t, Vector3D(1.0f, 1.0f, 1.0f));

    Vector3D dz(1.0f, 1.0f, 1.0f); dz /= 0.0f;
    CHECK((std::isinf(dz.x) && std::isinf(dz.y) && std::isinf(dz.z)));
    CHECK_FALSE((Vector3D(0.0f, NaN, 0.0f) == Vector3D(0.0f, NaN, 0.0f)));
}

// ===================================================================== Matrix3x3
TEST_CASE("Matrix3x3 basics", "[matrix]")
{
    CheckMat(Matrix3x3(), Matrix3x3::Scale(1.0f, 1.0f));   // default is identity

    const Matrix3x3 t = Matrix3x3::Translate(5.0f, 7.0f);
    CHECK(t.m[2][0] == Near(5.0f));                         // translation lives in the bottom row
    CHECK(t.m[2][1] == Near(7.0f));
    CheckNear(t.TransformPoint(Vector2D(1.0f, 2.0f)), Vector2D(6.0f, 9.0f));
    CheckNear(t.TransformDirection(Vector2D(1.0f, 2.0f)), Vector2D(1.0f, 2.0f));   // w = 0
    CHECK(t.Transpose().m[0][2] == Near(5.0f));
    CHECK(t.Transpose().m[1][2] == Near(7.0f));

    CheckNear(Matrix3x3::Scale(2.0f, 3.0f).TransformPoint(Vector2D(1.0f, 1.0f)), Vector2D(2.0f, 3.0f));
    CheckNear(Matrix3x3::Rotate(90.0f).TransformPoint(Vector2D(1.0f, 0.0f)), Vector2D(0.0f, 1.0f));

    const Matrix3x3 s = Matrix3x3::Scale(2.0f, 2.0f), t10 = Matrix3x3::Translate(10.0f, 0.0f);
    CheckNear((s * t10).TransformPoint(Vector2D(1.0f, 1.0f)), Vector2D(12.0f, 2.0f));   // scale first
    CheckNear((t10 * s).TransformPoint(Vector2D(1.0f, 1.0f)), Vector2D(22.0f, 2.0f));   // translate first

    const Matrix3x3 srt = Matrix3x3::SRT(Vector2D(2.0f, 2.0f), 90.0f, Vector2D(10.0f, 0.0f));
    CheckNear(srt.TransformPoint(Vector2D(1.0f, 0.0f)), Vector2D(10.0f, 2.0f));
    CheckMat(srt, Matrix3x3::Scale(2.0f, 2.0f) * Matrix3x3::Rotate(90.0f) * Matrix3x3::Translate(10.0f, 0.0f));
}

TEST_CASE("Matrix3x3 edge cases", "[matrix][edge]")
{
    const Matrix3x3 id;
    const Matrix3x3 a = Matrix3x3::SRT(Vector2D(2.0f, 1.0f), 30.0f, Vector2D(4.0f, -5.0f));
    const Matrix3x3 b = Matrix3x3::SRT(Vector2D(-1.0f, 2.0f), 75.0f, Vector2D(0.0f, 3.0f));
    const Matrix3x3 c = Matrix3x3::SRT(Vector2D(1.0f, 3.0f), -20.0f, Vector2D(2.0f, 2.0f));

    CheckMat(Matrix3x3::Translate(0.0f, 0.0f), id);
    CheckMat(Matrix3x3::Rotate(0.0f), id);
    CheckMat(Matrix3x3::Rotate(360.0f), id);
    CheckMat(Matrix3x3::Rotate(-360.0f), id);
    CheckMat(Matrix3x3::Rotate(450.0f), Matrix3x3::Rotate(90.0f));      // angles wrap
    CheckMat(Matrix3x3::Rotate(-90.0f), Matrix3x3::Rotate(270.0f));
    CheckNear(Matrix3x3::Rotate(180.0f).TransformPoint(Vector2D(1.0f, 0.0f)), Vector2D(-1.0f, 0.0f));
    CheckMat(Matrix3x3::Rotate(30.0f) * Matrix3x3::Rotate(60.0f), Matrix3x3::Rotate(90.0f));
    CheckMat(Matrix3x3::Rotate(37.0f) * Matrix3x3::Rotate(-37.0f), id);
    CheckMat(Matrix3x3::Rotate(37.0f) * Matrix3x3::Rotate(37.0f).Transpose(), id);   // rotation is orthonormal
    CHECK(Matrix3x3::Rotate(37.0f).TransformPoint(Vector2D(3.0f, 4.0f)).Magnitude() == Near(5.0f));
    CheckMat(a.Transpose().Transpose(), a);

    CheckMat(id * a, a);
    CheckMat(a * id, a);
    CheckMat((a * b) * c, a * (b * c));                                  // associative
    Matrix3x3 sq = a;
    sq = sq * sq;                                                        // result is also an input
    CheckMat(sq, a * a);

    CheckNear(Matrix3x3::Scale(0.0f, 0.0f).TransformPoint(Vector2D(5.0f, 7.0f)), Vector2D(0.0f, 0.0f));
    CheckNear(Matrix3x3::Scale(-1.0f, 1.0f).TransformPoint(Vector2D(2.0f, 3.0f)), Vector2D(-2.0f, 3.0f));
    CheckNear(Matrix3x3::Rotate(123.0f).TransformPoint(Vector2D(0.0f, 0.0f)), Vector2D(0.0f, 0.0f));
    CheckNear(Matrix3x3::Rotate(40.0f).TransformDirection(Vector2D(3.0f, 4.0f)), Matrix3x3::Rotate(40.0f).TransformPoint(Vector2D(3.0f, 4.0f)));
    CheckNear(Matrix3x3::SRT(Vector2D(0.0f, 0.0f), 45.0f, Vector2D(3.0f, 4.0f)).TransformPoint(Vector2D(9.0f, 9.0f)), Vector2D(3.0f, 4.0f));
}

// ===================================================================== Matrix4x4
TEST_CASE("Matrix4x4 basics", "[matrix]")
{
    CheckMat(Matrix4x4(), Matrix4x4::Scale(1.0f, 1.0f, 1.0f));

    const Matrix4x4 t = Matrix4x4::Translate(5.0f, 7.0f, 9.0f);
    CHECK(t.m[3][0] == Near(5.0f));
    CHECK(t.m[3][1] == Near(7.0f));
    CHECK(t.m[3][2] == Near(9.0f));
    CheckNear(t.TransformPoint(Vector3D(1.0f, 2.0f, 3.0f)), Vector3D(6.0f, 9.0f, 12.0f));
    CheckNear(t.TransformDirection(Vector3D(1.0f, 2.0f, 3.0f)), Vector3D(1.0f, 2.0f, 3.0f));
    CHECK(t.Transpose().m[0][3] == Near(5.0f));
    CHECK(t.Transpose().m[2][3] == Near(9.0f));

    CheckNear(Matrix4x4::Scale(2.0f, 3.0f, 4.0f).TransformPoint(Vector3D(1.0f, 1.0f, 1.0f)), Vector3D(2.0f, 3.0f, 4.0f));
    CheckNear(Matrix4x4::RotateX(90.0f).TransformPoint(Vector3D(0.0f, 1.0f, 0.0f)), Vector3D(0.0f, 0.0f, 1.0f));
    CheckNear(Matrix4x4::RotateY(90.0f).TransformPoint(Vector3D(0.0f, 0.0f, 1.0f)), Vector3D(1.0f, 0.0f, 0.0f));
    CheckNear(Matrix4x4::RotateY(90.0f).TransformPoint(Vector3D(1.0f, 0.0f, 0.0f)), Vector3D(0.0f, 0.0f, -1.0f));
    CheckNear(Matrix4x4::RotateZ(90.0f).TransformPoint(Vector3D(1.0f, 0.0f, 0.0f)), Vector3D(0.0f, 1.0f, 0.0f));

    CheckMat(Matrix4x4::Rotate(Vector3D(90.0f, 0.0f, 0.0f)), Matrix4x4::RotateX(90.0f));
    CheckNear(Matrix4x4::Rotate(Vector3D(90.0f, 90.0f, 0.0f)).TransformPoint(Vector3D(0.0f, 1.0f, 0.0f)), Vector3D(1.0f, 0.0f, 0.0f));   // X then Y

    const Matrix4x4 s = Matrix4x4::Scale(2.0f, 2.0f, 2.0f), t10 = Matrix4x4::Translate(10.0f, 0.0f, 0.0f);
    CheckNear((s * t10).TransformPoint(Vector3D(1.0f, 1.0f, 1.0f)), Vector3D(12.0f, 2.0f, 2.0f));
    CheckNear((t10 * s).TransformPoint(Vector3D(1.0f, 1.0f, 1.0f)), Vector3D(22.0f, 2.0f, 2.0f));

    const Matrix4x4 srt = Matrix4x4::SRT(Vector3D(2.0f, 2.0f, 2.0f), Vector3D(0.0f, 0.0f, 90.0f), Vector3D(10.0f, 0.0f, 0.0f));
    CheckNear(srt.TransformPoint(Vector3D(1.0f, 0.0f, 0.0f)), Vector3D(10.0f, 2.0f, 0.0f));
    CheckMat(srt, Matrix4x4::Scale(2.0f, 2.0f, 2.0f) * Matrix4x4::Rotate(Vector3D(0.0f, 0.0f, 90.0f)) * Matrix4x4::Translate(10.0f, 0.0f, 0.0f));
}

TEST_CASE("Matrix4x4 edge cases", "[matrix][edge]")
{
    const Matrix4x4 id;
    const Matrix4x4 a = Matrix4x4::SRT(Vector3D(2.0f, 1.0f, 3.0f), Vector3D(30.0f, 40.0f, 50.0f), Vector3D(4.0f, -5.0f, 6.0f));
    const Matrix4x4 b = Matrix4x4::SRT(Vector3D(-1.0f, 2.0f, 1.0f), Vector3D(75.0f, -20.0f, 10.0f), Vector3D(0.0f, 3.0f, -3.0f));
    const Matrix4x4 c = Matrix4x4::SRT(Vector3D(1.0f, 1.0f, 2.0f), Vector3D(5.0f, 60.0f, -45.0f), Vector3D(2.0f, 2.0f, 2.0f));
    const Matrix4x4 rot = Matrix4x4::Rotate(Vector3D(30.0f, 40.0f, 50.0f));

    CheckMat(Matrix4x4::Translate(0.0f, 0.0f, 0.0f), id);
    CheckMat(Matrix4x4::Rotate(Vector3D(0.0f, 0.0f, 0.0f)), id);
    CheckMat(Matrix4x4::Rotate(Vector3D(360.0f, 360.0f, 360.0f)), id);
    CheckMat(Matrix4x4::Rotate(Vector3D(450.0f, 0.0f, 0.0f)), Matrix4x4::RotateX(90.0f));
    CheckMat(Matrix4x4::Rotate(Vector3D(0.0f, 90.0f, 0.0f)), Matrix4x4::RotateY(90.0f));
    CheckMat(Matrix4x4::Rotate(Vector3D(0.0f, 0.0f, 90.0f)), Matrix4x4::RotateZ(90.0f));
    CheckMat(Matrix4x4::RotateX(30.0f) * Matrix4x4::RotateX(60.0f), Matrix4x4::RotateX(90.0f));
    CheckMat(Matrix4x4::RotateY(37.0f) * Matrix4x4::RotateY(-37.0f), id);

    CheckMat(rot * rot.Transpose(), id);                                 // orthonormal
    CHECK(rot.TransformPoint(Vector3D(1.0f, 2.0f, 2.0f)).Magnitude() == Near(3.0f));
    const Matrix4x4 lock = Matrix4x4::Rotate(Vector3D(10.0f, 90.0f, 20.0f));   // Y = 90: gimbal-lock pose
    CheckMat(lock * lock.Transpose(), id);
    CheckMat(a.Transpose().Transpose(), a);

    CheckMat(id * a, a);
    CheckMat(a * id, a);
    CheckMat((a * b) * c, a * (b * c));
    Matrix4x4 sq = a;
    sq = sq * sq;
    CheckMat(sq, a * a);

    CheckNear(Matrix4x4::Scale(0.0f, 0.0f, 0.0f).TransformPoint(Vector3D(5.0f, 7.0f, 9.0f)), Vector3D(0.0f, 0.0f, 0.0f));
    CheckNear(Matrix4x4::Scale(-1.0f, 1.0f, 1.0f).TransformPoint(Vector3D(2.0f, 3.0f, 4.0f)), Vector3D(-2.0f, 3.0f, 4.0f));
    CheckNear(rot.TransformPoint(Vector3D(0.0f, 0.0f, 0.0f)), Vector3D(0.0f, 0.0f, 0.0f));
    CheckNear(rot.TransformDirection(Vector3D(1.0f, 2.0f, 2.0f)), rot.TransformPoint(Vector3D(1.0f, 2.0f, 2.0f)));
    CheckNear(Matrix4x4::SRT(Vector3D(0.0f, 0.0f, 0.0f), Vector3D(10.0f, 20.0f, 30.0f), Vector3D(1.0f, 2.0f, 3.0f)).TransformPoint(Vector3D(9.0f, 9.0f, 9.0f)), Vector3D(1.0f, 2.0f, 3.0f));
}

// ===================================================================== Transforms
TEST_CASE("Transform2D", "[transform]")
{
    const Transform2D d;
    CHECK((d.scale == Vector2D(1.0f, 1.0f) && d.rotation == 0.0f && d.position == Vector2D(0.0f, 0.0f)));
    CheckMat(d.GetMatrix(), Matrix3x3());

    Transform2D t(Vector2D(2.0f, 2.0f), 90.0f, Vector2D(10.0f, 0.0f));
    CHECK((t.scale == Vector2D(2.0f, 2.0f) && t.rotation == 90.0f && t.position == Vector2D(10.0f, 0.0f)));
    CheckNear(t.GetMatrix().TransformPoint(Vector2D(1.0f, 0.0f)), Vector2D(10.0f, 2.0f));
    CheckMat(t.GetMatrix(), Matrix3x3::SRT(t.scale, t.rotation, t.position));

    // the matrix is derived from the fields every time, never cached
    const Matrix3x3 before = t.GetMatrix();
    t.position = Vector2D(-4.0f, 6.0f);
    CheckNear(t.GetMatrix().TransformPoint(Vector2D(1.0f, 0.0f)), Vector2D(-4.0f, 8.0f));
    CHECK(before.m[2][0] == Near(10.0f));                                // an old copy is unchanged
    t.rotation = 450.0f;
    CheckMat(t.GetMatrix(), Transform2D(t.scale, 90.0f, t.position).GetMatrix());

    CheckNear(Transform2D(Vector2D(0.0f, 0.0f), 45.0f, Vector2D(3.0f, 4.0f)).GetMatrix().TransformPoint(Vector2D(9.0f, 9.0f)), Vector2D(3.0f, 4.0f));
    CheckNear(Transform2D(Vector2D(-1.0f, 1.0f), 0.0f, Vector2D(0.0f, 0.0f)).GetMatrix().TransformPoint(Vector2D(2.0f, 3.0f)), Vector2D(-2.0f, 3.0f));
    CheckMat(Transform2D(Vector2D(1.0f, 1.0f), 360.0f, Vector2D(0.0f, 0.0f)).GetMatrix(), Matrix3x3());
}

TEST_CASE("Transform3D", "[transform]")
{
    const Transform3D d;
    CHECK((d.scale == Vector3D(1.0f, 1.0f, 1.0f) && d.rotation == Vector3D(0.0f, 0.0f, 0.0f) && d.position == Vector3D(0.0f, 0.0f, 0.0f)));
    CheckMat(d.GetMatrix(), Matrix4x4());

    Transform3D t(Vector3D(2.0f, 2.0f, 2.0f), Vector3D(0.0f, 0.0f, 90.0f), Vector3D(10.0f, 0.0f, 0.0f));
    CHECK((t.scale == Vector3D(2.0f, 2.0f, 2.0f) && t.rotation == Vector3D(0.0f, 0.0f, 90.0f) && t.position == Vector3D(10.0f, 0.0f, 0.0f)));
    CheckNear(t.GetMatrix().TransformPoint(Vector3D(1.0f, 0.0f, 0.0f)), Vector3D(10.0f, 2.0f, 0.0f));
    CheckMat(t.GetMatrix(), Matrix4x4::SRT(t.scale, t.rotation, t.position));

    const Matrix4x4 before = t.GetMatrix();
    t.position = Vector3D(-4.0f, 6.0f, 1.0f);
    CheckNear(t.GetMatrix().TransformPoint(Vector3D(1.0f, 0.0f, 0.0f)), Vector3D(-4.0f, 8.0f, 1.0f));
    CHECK(before.m[3][0] == Near(10.0f));
    t.rotation = Vector3D(0.0f, 0.0f, 450.0f);
    CheckMat(t.GetMatrix(), Transform3D(t.scale, Vector3D(0.0f, 0.0f, 90.0f), t.position).GetMatrix());

    CheckNear(Transform3D(Vector3D(0.0f, 0.0f, 0.0f), Vector3D(10.0f, 20.0f, 30.0f), Vector3D(1.0f, 2.0f, 3.0f)).GetMatrix().TransformPoint(Vector3D(9.0f, 9.0f, 9.0f)), Vector3D(1.0f, 2.0f, 3.0f));
    CheckNear(Transform3D(Vector3D(-1.0f, 1.0f, 1.0f), Vector3D(0.0f, 0.0f, 0.0f), Vector3D(0.0f, 0.0f, 0.0f)).GetMatrix().TransformPoint(Vector3D(2.0f, 3.0f, 4.0f)), Vector3D(-2.0f, 3.0f, 4.0f));
    CheckMat(Transform3D(Vector3D(1.0f, 1.0f, 1.0f), Vector3D(360.0f, 360.0f, 360.0f), Vector3D(0.0f, 0.0f, 0.0f)).GetMatrix(), Matrix4x4());
}
