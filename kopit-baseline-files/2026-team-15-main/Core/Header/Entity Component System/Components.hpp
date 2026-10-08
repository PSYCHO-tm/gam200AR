// components.hpp - plain data components. Rotation is in degrees. Positions are marker-local meters.
#pragma once
#include "Math/Vec3.hpp"
#include <string>

namespace Core
{
	struct Name
	{
		std::string Value;
	};

	struct Transform
	{
		Vec3 Position{};
		Vec3 PrevPosition{}; // previous tick, for render interpolation
		Vec3 Rotation{};     // degrees, applied Y then X then Z
		Vec3 PrevRotation{}; // previous tick, for render interpolation
		Vec3 Scale{1.0f, 1.0f, 1.0f};
		Vec3 PrevScale{1.0f, 1.0f, 1.0f};
	};

	struct MeshRef
	{
		std::string Path; // relative to the data folder, e.g. meshes/testCube.obj
	};

	struct Material
	{
		Vec3 Color{0.8f, 0.8f, 0.8f};
	};

	struct Spinner
	{
		float DegreesPerSecond = 0.0f;
	};
}
