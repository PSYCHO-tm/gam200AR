#pragma once

#include "../Math/Mat4.hpp"
#include "../Math/Vec3.hpp"

namespace Core
{
	struct Ray
	{
		Vec3 Origin;
		Vec3 Direction = {0.0f, 0.0f, -1.0f};
	};

	struct Aabb
	{
		Vec3 Min;
		Vec3 Max;
	};

	// Slab test. A ray that starts inside the box hits at distance 0. Boxes behind the origin are misses.
	bool IntersectRayAabb(Ray const& ray, Aabb const& box, float& outDistance);

	// Axis-aligned bounds of a box after an affine transform (all 8 corners are transformed).
	Aabb TransformAabb(Aabb const& box, Mat4 const& m);
}
