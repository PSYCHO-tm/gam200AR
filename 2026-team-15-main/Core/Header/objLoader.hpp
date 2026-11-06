#pragma once

#include <optional>
#include <string>

#include "Geometry.hpp"
#include "Vector.hpp"

// The Render-System's way in (whiteboard "John Fix"): the rendering code asks ObjLoader for a piece's MESH (ParseObj)
// and for HOW to draw it (MakeMaterial), instead of reaching into Shader, Mesh and Geometry itself.
// Plain C++ with no OpenGL, so the ECS checks and the console/CI build can use it too.

// Wavefront OBJ text: v, vt, vn, f (v, v/vt, v//vn, v/vt/vn), negative indices, polygons (fan triangulated).
// Missing normals are generated. Returns nullopt for a bad index or no geometry.
// Takes the file's TEXT, not a path: on Android the files are baked into the app by CMake (see EmbedAssets.cmake).
std::optional<MeshData> ParseObj(std::string const& text);

// Which shader program draws a piece. One for now; FreeType text will add its own.
enum class ShaderKind
{
    Lit, // lit.vert + lit.frag: one colour, a fixed sun light, optional highlight, see-through and depth hiding
};

// The whiteboard's box: how a piece is drawn, kept apart from its mesh (one mesh can be drawn in different materials).
struct RenderMaterial
{
    ShaderKind shader = ShaderKind::Lit;
    Vector3D   colour{0.8f, 0.8f, 0.8f}; // base colour, each 0..1
};

// The shader for a name from the data ("Lit"), or nullopt if there is no shader by that name.
std::optional<ShaderKind> ShaderKindFromName(std::string const& name);

// Builds the box from the values in a MeshRef. nullopt when the shader name is unknown (the caller logs which piece).
// Colour values are clamped to 0..1.
std::optional<RenderMaterial> MakeMaterial(std::string const& shaderName, Vector3D const& colour);
