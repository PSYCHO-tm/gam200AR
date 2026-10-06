#pragma once

#include "Vec3.hpp"

namespace Core
{
	// Column-major 4x4 matrix, the same layout OpenGL and ARCore use
	// (ArCamera_getViewMatrix / ArCamera_getProjectionMatrix can be copied straight in).
	struct Mat4
	{
		float Values[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	};

	Mat4 Multiply(Mat4 const& a, Mat4 const& b); // a * b
	Mat4 Translate(Vec3 t);
	Mat4 ScaleUniform(float s);
	Mat4 RotateY(float radians);
	Mat4 Perspective(float fovYRadians, float aspect, float nearPlane, float farPlane);
	Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up);
	Mat4 Inverse(Mat4 const& m, bool& ok);

	Vec3 TransformPoint(Mat4 const& m, Vec3 p);             // affine, ignores w
	Vec3 UnprojectPoint(Mat4 const& invViewProj, Vec3 ndc); // divides by w
}
