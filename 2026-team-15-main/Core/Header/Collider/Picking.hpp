#pragma once

#include <vector>

#include "Aabb.hpp"

namespace Core
{
	struct PickResult
	{
		int   Index    = -1; // -1 means nothing was hit
		float Distance = 0.0f;
	};

	// Turns a touch point (pixels, origin top-left) into a world-space ray.
	// On the phone, view and projection come from ARCore for the current frame; on desktop from the debug camera.
	Ray ScreenPointToRay(float pixelX, float pixelY, float viewportWidth, float viewportHeight, Mat4 const& view,
	                     Mat4 const& projection);

	PickResult PickNearest(Ray const& ray, std::vector<Aabb> const& worldBounds);
}
