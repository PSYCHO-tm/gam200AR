#pragma once
#include <cstddef>
#include <optional>
#include <vector>

#include "Geometry.hpp"
#include "Input.hpp"
#include "Matrix.hpp"
#include "equipmentDef.hpp" // exact case, for Linux builds
#include "objLoader.hpp"    // RenderMaterial

// The equipment you see in AR: loads it, keeps it still in the room, selects it when you tap it, and draws it.
// Everything runs on the GL thread, called from Core (Init / Update / Render), so nothing here needs a lock.
//
// There is exactly one EquipmentScene, reached through Get(). Nobody owns it.
//
// Data in: the pieces come from ECS entities that have MeshRef + Transform3D (Engine.cpp, BuildEquipmentLibrary), their
// .obj meshes from the app (baked in by CMake). Placement in: the anchor pose from ARCore (Engine.cpp,
// ARTrackingState). Out: a list of Renderer::MeshDraw items each frame (the Renderer does the GL work), and the
// selected piece for the action buttons (Engine.cpp, GetActionsText).
//
// Spaces: a piece's mesh is in its own "local" space; Equipment.json places it relative to the anchor; the anchor pose
// places that in the ARCore world. Row vectors, so local * localModel * anchorPose = world (Matrix.hpp).
class EquipmentScene
{
public:
    static EquipmentScene& Get(); // the one scene (created the first time this is called)

    EquipmentScene(EquipmentScene const&) = delete; // one scene only: no copies
    EquipmentScene& operator=(EquipmentScene const&) = delete;

    // Once per GL context. Takes the equipment list built from the ECS entities, and reads the meshes that CMake baked
    // into the app. Returns false (and logs why) if anything is missing or invalid.
    bool Init(EquipmentLibrary loadedLibrary);

    // Once per frame. 'view' and 'projection' are the camera the player is looking through (the last frame drawn),
    // so a tap selects exactly what is on screen. 'tracking' is false while ARCore cannot place anything.
    void Update(float dt, InputSnapshot const& input, bool tracking, Matrix4x4 const& view, Matrix4x4 const& projection,
                int viewportW, int viewportH);

    // Once per frame, after the camera picture. Does nothing until the equipment has been placed.
    void Draw(Matrix4x4 const& view, Matrix4x4 const& projection);

    // false = the station is hidden (tracking cannot be trusted, or the card is covered): nothing is drawn or picked.
    void SetStationShown(bool shown) { StationShown = shown; }

    // Called every frame while ARCore tracks, with the anchor's current pose (ARCore keeps correcting it).
    // The first call places the pieces (before it nothing is drawn); later calls move them with the anchor.
    // A desktop build with no ARCore can call it once with a fixed pose (the "fake anchor").
    void SetAnchorPose(Matrix4x4 const& pose);

    // The small debug square on the plane. 0 = hide, 1 = plane not usable (red), 2 = good plane (yellow),
    // 3 = placed, at the anchor (green). 'pose' is where it lies, +y being the plane's normal.
    void SetMarker(int state, Matrix4x4 const& pose);

    // The planes ARCore is tracking right now, each drawn as a see-through rectangle so you can see what it found.
    // 'poses' are the plane centres (+y is the normal) and 'extents' their size along their own x and z, in metres.
    void SetPlanes(std::vector<Matrix4x4> const& poses, std::vector<Vector2D> const& extents);

    // For the buttons and text that will be added later: the equipment, and which piece (if any) is selected.
    EquipmentLibrary const& GetLibrary() const { return Library; }
    std::optional<std::size_t> GetSelected() const { return Selected; }

private:
    EquipmentScene() = default; // only Get() makes one

    static MeshData MakeMarkerMesh(); // a small flat square, lying in the x-z plane (debug square, planes)

    void RefreshModels(); // models[i] = localModels[i] * anchorPose

    static char const*
    FindAsset(char const* path); // an embedded file's text by repo path; nullptr on desktop / missing

    EquipmentLibrary Library;              // the pieces, sorted by id (from the ECS)
    std::vector<MeshData> Meshes;          // same order as library.items (CPU copy, for picking and bounds)
    std::vector<std::size_t> PieceMeshes;  // same order: each piece's mesh id in the Renderer (from AddMesh)
    std::size_t SquareMesh = 0;            // the Renderer's id for MakeMarkerMesh (debug square, planes)
    std::vector<RenderMaterial> Materials; // same order: each piece's {Shader, Colour}, from ObjLoader::MakeMaterial
    std::vector<Matrix4x4> LocalModels;    // local -> anchor-local (scale, yaw, position from the JSON), same order
    std::vector<Matrix4x4> Models;         // local -> ARCore world = localModels * anchorPose, same order
    Matrix4x4 AnchorPose;                  // set by SetAnchorPose
    std::optional<std::size_t> Selected;   // index into library.items of the tapped piece, if any
    std::vector<Matrix4x4> PlaneModels;    // each tracked plane as a scaled rectangle, local -> ARCore world
    int MarkerState = 0;                   // see SetMarker
    Matrix4x4 MarkerPose;                  // where the debug square lies
    bool Loaded = false;                   // Init succeeded: meshes read and handed to the Renderer
    bool Placed = false;                   // an anchor pose has arrived at least once
    bool StationShown = false;             // see SetStationShown
};