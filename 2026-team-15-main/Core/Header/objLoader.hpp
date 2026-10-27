#pragma once

#include <optional>
#include <string>

#include "Geometry.hpp"

// Wavefront OBJ text: v, vt, vn, f (v, v/vt, v//vn, v/vt/vn), negative indices, polygons (fan triangulated).
// Missing normals are generated. Returns nullopt for a bad index or no geometry.
// Takes the file's TEXT, not a path: on Android the files are baked into the app by CMake (see EmbedAssets.cmake).
std::optional<MeshData> ParseObj(std::string const& text);