#include "aabb.hpp"

#include <algorithm>
#include <cmath>

namespace Sim
{
	Aabb TransformAabb(Aabb const& box, Mat4 const& transform)
	{
		Vec3 low{1e30f, 1e30f, 1e30f};
		Vec3 high{-1e30f, -1e30f, -1e30f};
		for (int i = 0; i < 8; ++i)
		{
			Vec3 const corner{(i & 1) ? box.Max.x : box.Min.x, (i & 2) ? box.Max.y : box.Min.y,
			                  (i & 4) ? box.Max.z : box.Min.z};
			Vec3 const world = TransformPoint(transform, corner);
			low              = {std::min(low.x, world.x), std::min(low.y, world.y), std::min(low.z, world.z)};
			high             = {std::max(high.x, world.x), std::max(high.y, world.y), std::max(high.z, world.z)};
		}
		return {low, high};
	}

	bool RayIntersectsAabb(Ray const& ray, Aabb const& box, float& distance)
	{
		float const origin[3] = {ray.Origin.x, ray.Origin.y, ray.Origin.z};
		float const dir[3]    = {ray.Direction.x, ray.Direction.y, ray.Direction.z};
		float const low[3]    = {box.Min.x, box.Min.y, box.Min.z};
		float const high[3]   = {box.Max.x, box.Max.y, box.Max.z};

		float entry = 0.0f;
		float exit  = 1e30f;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::fabs(dir[axis]) < 1e-12f)
			{
				if (origin[axis] < low[axis] || origin[axis] > high[axis])
				{
					return false; // parallel to this slab and outside it
				}
				continue;
			}
			float near = (low[axis] - origin[axis]) / dir[axis];
			float far  = (high[axis] - origin[axis]) / dir[axis];
			if (near > far)
			{
				std::swap(near, far);
			}
			entry = std::max(entry, near);
			exit  = std::min(exit, far);
			if (entry > exit)
			{
				return false;
			}
		}
		distance = entry;
		return true;
	}
}
