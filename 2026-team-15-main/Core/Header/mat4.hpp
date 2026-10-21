#pragma once

#include "vectors.hpp"

namespace Sim
{
	// Column-major 4x4 (OpenGL layout): Values[column * 4 + row]. Default is identity.
	struct Mat4
	{
		float Values[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	};

	Mat4 Multiply(Mat4 const& a, Mat4 const& b); // a * b: b is applied first
	Mat4 Translate(Vec3 offset);
	Mat4 Scale(Vec3 factors);
	Mat4 RotateXDeg(float degrees);
	Mat4 RotateYDeg(float degrees);
	Mat4 RotateZDeg(float degrees);
	Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up);
	// OpenGL clip space (z from -1 to 1). fovYDeg is the full vertical field of view.
	Mat4 Perspective(float fovYDeg, float aspect, float nearPlane, float farPlane);
	// Returns identity if the matrix is not invertible.
	Mat4 Inverse(Mat4 const& m);

	Vec3 TransformPoint(Mat4 const& m, Vec3 p);     // divides by w
	Vec3 TransformDirection(Mat4 const& m, Vec3 d); // ignores translation
}
