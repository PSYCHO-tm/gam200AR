#pragma once

#include <cmath>

// Small value types for the pure simulation layer. No GL, no platform headers.
namespace Sim
{
	struct Vec2
	{
		float x = 0.0f;
		float y = 0.0f;
	};

	struct Vec3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
	inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
	inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
	inline Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }

	inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

	inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

	inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }

	inline Vec3 Normalize(Vec3 a)
	{
		float const len = Length(a);
		return len > 1e-8f ? a / len : Vec3{0.0f, 0.0f, 0.0f};
	}
}
