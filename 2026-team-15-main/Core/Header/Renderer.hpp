#pragma once
#include <cstddef>
#include <vector>

#include "Geometry.hpp"
#include "Matrix.hpp"
#include "Mesh.hpp"
#include "Shader.hpp"
#include "Vector.hpp"

// Owns the lit shader and every GPU mesh. Every function runs on the GL thread.
// Knows only engine types (MeshData, Matrix4x4, Vector3D): callers turn their own data into these first.
// Matrices use the engine convention (Matrix.hpp): row vectors, v' = v * M.
//
// There is exactly one Renderer, reached through Get(). Nobody owns it, so any module can draw without being handed one.
class Renderer
{
public:
    // One mesh to draw this frame, with its placement and colours.
    struct MeshDraw
    {
        std::size_t meshIndex = 0;          // which mesh, by its position in the list given to SetMeshes
        Matrix4x4   model;                  // local -> world (identity by default)
        Vector3D    color{1.0f, 1.0f, 1.0f};
        Vector3D    tint{0.0f, 0.0f, 0.0f};
        float       tintAmount = 0.0f;      // 0 = plain colour, 1 = fully the tint colour
        float       alpha = 1.0f;           // 1 = solid, below 1 = see-through (blended with what is behind it)
    };

    static Renderer& Get();

    Renderer(Renderer const&)            = delete;
    Renderer& operator=(Renderer const&) = delete;

    // Call once per new GL context (onSurfaceCreated, via Core::Init). Forgets old GPU handles, builds the shader.
    bool Init();

    // The GL context was destroyed: forget GPU handles without deleting (the driver already freed them).
    void ContextLost();

    // App closing while the context is still alive: free everything.
    void Shutdown();

    // Replaces the meshes the renderer knows about (one list for now).
    // A CPU copy is kept, so they re-upload by themselves after a context loss.
    void SetMeshes(std::vector<MeshData> newMeshes);

    // Draws the given items. Clears depth first and leaves depth testing off afterwards.
    void DrawMeshes(std::vector<MeshDraw> const& items, Matrix4x4 const& view, Matrix4x4 const& projection);

private:
    Renderer() = default;

    void FreeMeshes();
    void UploadMeshes();

    Shader                litShader;
    std::vector<MeshData> meshData;    // CPU copy: the source for every (re)upload
    std::vector<Mesh>     meshes;      // GPU copy: rebuilt from meshData
    bool                  needsUpload = true;
};