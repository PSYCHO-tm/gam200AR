#pragma once
#include <cstdint>
#include <vector>

#include "Vector.hpp"

// The CPU-side shape of a mesh, before it goes to the GPU (Mesh.hpp). Plain data, no GL, so it can be built and
// tested without a window (ObjLoader, MakeQuad, EquipmentScene::MakeMarkerMesh all produce it).

// One corner of a triangle.
struct Vertex
{
    Vector3D Pos{0.0f, 0.0f, 0.0f};    // position in the model's own space, metres
    Vector3D Normal{0.0f, 1.0f, 0.0f}; // which way the surface faces here (unit length), used for lighting
    Vector2D Uv{0.0f, 0.0f};           // texture coordinate, 0..1 (kept, but not uploaded yet: see Mesh.hpp)
};

// A whole mesh: shared corners plus the triangles that use them.
struct MeshData
{
    std::vector<Vertex> Vertices;       // each corner once, even when several triangles share it
    std::vector<std::uint32_t> Indices; // 3 per triangle
};

// A 1x1 square from (0,0) to (1,1) in the z = 0 plane, facing +z, UV (0,0) to (1,1).
// Scale and move it with a Transform2D to make any button.
MeshData MakeQuad();
