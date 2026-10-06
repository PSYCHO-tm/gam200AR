#include "../../Header/Collider/Picking.hpp"

namespace Core
{
	Ray ScreenPointToRay(float pixelX, float pixelY, float viewportWidth, float viewportHeight, Mat4 const& view,
	                     Mat4 const& projection)
	{
		bool       ok  = false;
		Mat4 const inv = Inverse(Multiply(projection, view), ok);
		if (!ok || viewportWidth <= 0.0f || viewportHeight <= 0.0f)
		{
			return Ray{};
		}
		float const ndcX = 2.0f * pixelX / viewportWidth - 1.0f;
		float const ndcY = 1.0f - 2.0f * pixelY / viewportHeight;

		Vec3 const nearPoint = UnprojectPoint(inv, {ndcX, ndcY, -1.0f});
		Vec3 const farPoint  = UnprojectPoint(inv, {ndcX, ndcY, 1.0f});

		Ray ray;
		ray.Origin    = nearPoint;
		ray.Direction = Normalize(farPoint - nearPoint);
		return ray;
	}

	PickResult PickNearest(Ray const& ray, std::vector<Aabb> const& worldBounds)
	{
		PickResult best;
		for (std::size_t i = 0; i < worldBounds.size(); ++i)
		{
			float distance = 0.0f;
			if (IntersectRayAabb(ray, worldBounds[i], distance) && (best.Index < 0 || distance < best.Distance))
			{
				best.Index    = static_cast<int>(i);
				best.Distance = distance;
			}
		}
		return best;
	}
}
