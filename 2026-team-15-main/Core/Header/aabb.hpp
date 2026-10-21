#pragma once

#include "mat4.hpp"
#include "vectors.hpp"

namespace Sim
{
	struct Aabb
	{
		Vec3 Min{0.0f, 0.0f, 0.0f};
		Vec3 Max{0.0f, 0.0f, 0.0f};
	};

	struct Ray
	{
		Vec3 Origin{0.0f, 0.0f, 0.0f};
		Vec3 Direction{0.0f, 0.0f, -1.0f}; // unit length
	};

	// World-space box that encloses the 8 transformed corners of 'box'.
	Aabb TransformAabb(Aabb const& box, Mat4 const& transform);

	// Slab test. On a hit, distance receives the distance along the ray to the entry point
	// (0 when the origin is inside the box).
	bool RayIntersectsAabb(Ray const& ray, Aabb const& box, float& distance);
}
