#include "../../Header/Collider/Aabb.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Core
{
	bool IntersectRayAabb(Ray const& ray, Aabb const& box, float& outDistance)
	{
		float const origin[3] = {ray.Origin.x, ray.Origin.y, ray.Origin.z};
		float const dir[3]    = {ray.Direction.x, ray.Direction.y, ray.Direction.z};
		float const lo[3]     = {box.Min.x, box.Min.y, box.Min.z};
		float const hi[3]     = {box.Max.x, box.Max.y, box.Max.z};

		float tMin = 0.0f;
		float tMax = std::numeric_limits<float>::max();
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::fabs(dir[axis]) < 1e-8f)
			{
				if (origin[axis] < lo[axis] || origin[axis] > hi[axis])
				{
					return false;
				}
				continue;
			}
			float t1 = (lo[axis] - origin[axis]) / dir[axis];
			float t2 = (hi[axis] - origin[axis]) / dir[axis];
			if (t1 > t2)
			{
				std::swap(t1, t2);
			}
			tMin = std::max(tMin, t1);
			tMax = std::min(tMax, t2);
			if (tMin > tMax)
			{
				return false;
			}
		}
		outDistance = tMin;
		return true;
	}

	Aabb TransformAabb(Aabb const& box, Mat4 const& m)
	{
		Aabb result;
		bool first = true;
		for (int i = 0; i < 8; ++i)
		{
			Vec3 const corner = {(i & 1) ? box.Max.x : box.Min.x, (i & 2) ? box.Max.y : box.Min.y,
			                     (i & 4) ? box.Max.z : box.Min.z};
			Vec3 const p = TransformPoint(m, corner);
			if (first)
			{
				result.Min = p;
				result.Max = p;
				first      = false;
				continue;
			}
			result.Min = {std::min(result.Min.x, p.x), std::min(result.Min.y, p.y), std::min(result.Min.z, p.z)};
			result.Max = {std::max(result.Max.x, p.x), std::max(result.Max.y, p.y), std::max(result.Max.z, p.z)};
		}
		return result;
	}
}
