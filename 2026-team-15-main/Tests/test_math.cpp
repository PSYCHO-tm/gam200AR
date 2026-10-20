#include <cmath>

#include "sim/math/mat4.hpp"
#include "testCheck.hpp"

using namespace Sim;

static bool Near(Vec3 a, Vec3 b, float eps = 1e-4f)
{
	return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}

int main()
{
	// Translate, scale and multiplication order (b is applied first).
	TEST_CHECK(Near(TransformPoint(Translate({1, 2, 3}), {0, 0, 0}), {1, 2, 3}));
	TEST_CHECK(Near(TransformPoint(Scale({2, 3, 4}), {1, 1, 1}), {2, 3, 4}));
	Mat4 const scaleThenMove = Multiply(Translate({10, 0, 0}), Scale({2, 2, 2}));
	TEST_CHECK(Near(TransformPoint(scaleThenMove, {1, 0, 0}), {12, 0, 0}));

	// Rotations: +90 about Z sends +x to +y, +90 about X sends +y to +z, +90 about Y sends +z to +x.
	TEST_CHECK(Near(TransformPoint(RotateZDeg(90), {1, 0, 0}), {0, 1, 0}));
	TEST_CHECK(Near(TransformPoint(RotateXDeg(90), {0, 1, 0}), {0, 0, 1}));
	TEST_CHECK(Near(TransformPoint(RotateYDeg(90), {0, 0, 1}), {1, 0, 0}));
	// A direction ignores translation.
	TEST_CHECK(Near(TransformDirection(Translate({5, 5, 5}), {1, 0, 0}), {1, 0, 0}));

	// Inverse round trip, and a singular matrix falls back to identity.
	Mat4 const m = Multiply(Translate({1, 2, 3}), Multiply(RotateYDeg(30), Scale({2, 1, 0.5f})));
	TEST_CHECK(Near(TransformPoint(Multiply(Inverse(m), m), {0.3f, -0.7f, 2.0f}), {0.3f, -0.7f, 2.0f}));
	TEST_CHECK(Near(TransformPoint(Inverse(Scale({0, 1, 1})), {1, 2, 3}), {1, 2, 3}));

	// LookAt puts the eye at the origin and the target straight ahead (-z).
	Mat4 const view = LookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
	TEST_CHECK(Near(TransformPoint(view, {0, 0, 5}), {0, 0, 0}));
	TEST_CHECK(Near(TransformPoint(view, {0, 0, 0}), {0, 0, -5}));
	TEST_CHECK(Near(TransformPoint(view, {1, 0, 0}), {1, 0, -5}));

	// Perspective: near and far planes land on -1 and +1, the view axis stays centered.
	Mat4 const proj = Perspective(60.0f, 2.0f, 0.1f, 100.0f);
	TEST_CHECK(Near(TransformPoint(proj, {0, 0, -0.1f}), {0, 0, -1}, 1e-3f));
	TEST_CHECK(Near(TransformPoint(proj, {0, 0, -100.0f}), {0, 0, 1}, 1e-3f));
	return TestFinish("test_math");
}
