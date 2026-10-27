#pragma once
#include <cstdint>
#include <vector>

#include "Vector.hpp"

struct Vertex
{
    Vector3D pos{0.0f, 0.0f, 0.0f};
    Vector3D normal{0.0f, 1.0f, 0.0f};
    Vector2D uv{0.0f, 0.0f};
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices; // 3 per triangle
};

// A 1x1 square from (0,0) to (1,1) in the z = 0 plane, facing +z, UV (0,0) to (1,1).
// Scale and move it with a Transform2D to make any button.
MeshData MakeQuad();
