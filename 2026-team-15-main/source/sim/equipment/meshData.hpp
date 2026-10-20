#pragma once

#include <cstdint>
#include <vector>

#include "sim/collider/aabb.hpp"
#include "sim/math/vectors.hpp"

namespace Sim
{
	struct Vertex
	{
		Vec3 Pos;
		Vec3 Normal{0.0f, 1.0f, 0.0f};
		Vec2 Uv;
	};

	struct MeshData
	{
		std::vector<Vertex>        Vertices;
		std::vector<std::uint32_t> Indices; // triangles
	};

	Aabb ComputeBounds(MeshData const& mesh);
}
