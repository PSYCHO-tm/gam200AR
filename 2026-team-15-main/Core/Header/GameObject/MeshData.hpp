#pragma once

#include <string>
#include <vector>

#include "../Collider/Aabb.hpp"

namespace Core
{
	// Triangle soup, 6 floats per vertex (position xyz, normal xyz), 3 vertices per triangle.
	struct MeshData
	{
		std::vector<float> Vertices;
		Aabb               Bounds;

		std::size_t VertexCount() const { return Vertices.size() / 6; }
	};

	// Minimal Wavefront OBJ reader: v, vn and f lines (v, v/vt, v//vn, v/vt/vn), faces triangulated as fans.
	// Missing normals become flat face normals. Anything else in the file is ignored.
	bool ParseObj(std::string const& text, MeshData& outMesh, std::string& outError);
}
