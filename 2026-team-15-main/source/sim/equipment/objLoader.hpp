#pragma once

#include <optional>
#include <string>

#include "sim/equipment/meshData.hpp"

namespace Sim
{
	// Wavefront OBJ: v, vt, vn, f (v, v/vt, v//vn, v/vt/vn), negative indices, polygons (fan triangulated).
	// Missing normals are generated. Returns nullopt for a missing file, a bad index or no geometry.
	std::optional<MeshData> LoadObj(std::string const& path);
	std::optional<MeshData> ParseObj(std::string const& text);
}
