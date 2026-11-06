#pragma once
#include <cstddef>
#include <vector>

#include "Geometry.hpp"
#include "Matrix.hpp"
#include "Mesh.hpp"
#include "Shader.hpp"
#include "Vector.hpp"
#include "objLoader.hpp" // ShaderKind: the Render-System gets "which shader" from ObjLoader's material box

// ---------------------------------------------------------------------------------------------------------------------
// Renderer: the "how" of drawing 3D things.
//
// Where it sits in a frame (Engine.cpp, Core::Render):
//     ArCameraDraw()            camera picture, the back layer (ARcore.cpp)
//     EquipmentScene::Draw()    decides WHAT to draw and WHERE -> a list of MeshDraw items
//       Renderer::DrawMeshes()  HOW: shader, uniforms, depth test, blending, one draw call per item
//         Mesh::Draw()          the actual glDrawElements
//
// Life cycle: Init() on every new GL context -> each module AddMesh()es its meshes once -> DrawMeshes() every frame ->
// Shutdown() on exit. ContextLost() covers Android throwing the GL context away (app sent to the background): the CPU
// copies survive and everything re-uploads by itself on the next draw.
//
// Modules: the Renderer is a shared mesh registry. Each drawing module (EquipmentScene, Loader, later Labels ...) adds
// its own meshes and keeps the ids it gets back, so modules never need to know about each other's meshes.
// ---------------------------------------------------------------------------------------------------------------------
//
// Owns the lit shader and every GPU mesh. Every function runs on the GL thread.
// Knows only engine types (MeshData, Matrix4x4, Vector3D): callers turn their own data into these first.
// Matrices use the engine convention (Matrix.hpp): row vectors, v' = v * M.
//
// There is exactly one Renderer, reached through Get(). Nobody owns it, so any module can draw without being handed
// one.
class Renderer
{
public:
    // A mesh's id, as returned by AddMesh. Valid for the whole run of the app, across GL context losses.
    using MeshId = std::size_t;

    // One mesh to draw this frame, with its placement and colours.
    struct MeshDraw
    {
        MeshId MeshIndex = 0;                // which mesh: the id AddMesh returned for it
        SHADER_KIND Kind = SHADER_KIND::LIT; // which program draws it (the material's shader, from ObjLoader)
        Matrix4x4 Model;                     // local -> world (identity by default)
        Vector3D Color{1.0f, 1.0f, 1.0f};
        Vector3D Tint{0.0f, 0.0f, 0.0f};
        float TintAmount = 0.0f; // 0 = plain colour, 1 = fully the tint colour
        float Alpha = 1.0f;      // 1 = solid, below 1 = see-through (blended with what is behind it)
        bool Occludable = true;  // true = hidden behind real things when there is depth (false for screen-space items)
    };

    // The one Renderer (created the first time this is called).
    static Renderer& Get();

    // Not copyable: there is one set of GPU handles, and two owners would free them twice.
    Renderer(Renderer const&) = delete;
    Renderer& operator=(Renderer const&) = delete;

    // Call once per new GL context (onSurfaceCreated, via Core::Init). Forgets old GPU handles, builds the shader.
    bool Init();

    // The GL context was destroyed: forget GPU handles without deleting (the driver already freed them).
    void ContextLost();

    // App closing while the context is still alive: free everything.
    void Shutdown();

    // Adds one mesh and returns its id. Call once per mesh for the life of the app (not per GL context): a CPU copy is
    // kept, uploaded on the next draw, and re-uploaded by itself after a context loss.
    MeshId AddMesh(MeshData mesh);

    // Draws the given items. Clears depth first and leaves depth testing off afterwards.
    void DrawMeshes(std::vector<MeshDraw> const& items, Matrix4x4 const& view, Matrix4x4 const& projection);

    // The real world's depth for this frame (from ARCore), so pieces are hidden behind a hand. 0 = no depth: draw
    // everything. 'uvTransform' is 9 floats, column-major: (screen x, screen y, 1) in 0..1 -> depth picture 0..1. Call
    // before DrawMeshes.
    void SetDepth(unsigned int texture, float const* uvTransform);

private:
    Renderer() = default; // only Get() makes one

    void FreeMeshes();                   // deletes every GPU mesh (needs a live context)
    void UploadMeshes();                 // uploads the meshes not on the GPU yet; runs lazily inside DrawMeshes
    Shader& ShaderFor(SHADER_KIND kind); // the program behind a material's shader name

    Shader LitShader; // lit.vert + lit.frag: one colour per mesh, simple sun light, optional depth hiding
    std::vector<MeshData> CpuMeshes; // CPU copy, indexed by MeshId: the source for every (re)upload
    std::vector<Mesh> GpuMeshes;     // GPU copy, same order; shorter than meshData until the next draw uploads the rest
    unsigned int DepthTexture = 0;   // owned by ARCore's side: only borrowed for this frame
    float DepthUv[9]{};              // see SetDepth
};