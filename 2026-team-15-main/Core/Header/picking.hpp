#pragma once

#include <string>
#include <vector>



// namespace Sim
// {
// 	// Pixel (0,0) is the top-left of the viewport. Uses the same view and projection the renderer draws with,
// 	// so a touch selects exactly what the player sees.
// 	Ray ScreenPointToRay(float pixelX, float pixelY, int viewportW, int viewportH, Mat4 const& view,
// 	                     Mat4 const& projection);

// 	struct PickCandidate
// 	{
// 		std::string Id;
// 		Aabb        WorldBounds;
// 	};

// 	struct PickResult
// 	{
// 		bool        Hit = false;
// 		std::string Id;
// 		float       Distance = 0.0f;
// 	};

// 	// Nearest candidate the ray hits, or Hit == false.
// 	PickResult PickNearest(Ray const& ray, std::vector<PickCandidate> const& candidates);
// }

