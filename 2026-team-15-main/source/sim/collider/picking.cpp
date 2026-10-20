#include "sim/collider/picking.hpp"

namespace Sim
{
	Ray ScreenPointToRay(float pixelX, float pixelY, int viewportW, int viewportH, Mat4 const& view,
	                     Mat4 const& projection)
	{
		float const ndcX = 2.0f * pixelX / static_cast<float>(viewportW) - 1.0f;
		float const ndcY = 1.0f - 2.0f * pixelY / static_cast<float>(viewportH);

		Mat4 const inverse = Inverse(Multiply(projection, view));
		Vec3 const nearPt  = TransformPoint(inverse, {ndcX, ndcY, -1.0f});
		Vec3 const farPt   = TransformPoint(inverse, {ndcX, ndcY, 1.0f});
		return {nearPt, Normalize(farPt - nearPt)};
	}

	PickResult PickNearest(Ray const& ray, std::vector<PickCandidate> const& candidates)
	{
		PickResult best;
		for (PickCandidate const& candidate : candidates)
		{
			float distance = 0.0f;
			if (RayIntersectsAabb(ray, candidate.WorldBounds, distance) && (!best.Hit || distance < best.Distance))
			{
				best = {true, candidate.Id, distance};
			}
		}
		return best;
	}
}
