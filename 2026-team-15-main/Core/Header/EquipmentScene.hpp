#pragma once
#include <cstddef>
#include <optional>
#include <vector>

#include "Geometry.hpp"
#include "Input.hpp"
#include "Matrix.hpp"
#include "EquipmentDef.hpp"
#include "Renderer.hpp"

// The equipment you see in AR: loads it, keeps it still in the room, selects it when you tap it, and draws it.
// Everything runs on the GL thread, called from Core (Init / Update / Render), so nothing here needs a lock.
//
// There is exactly one EquipmentScene, reached through Get(). Nobody owns it.
//
// Data in: the pieces come from ECS entities that have MeshRef + Transform3D (Engine.cpp, BuildEquipmentLibrary), their
// .obj meshes from the app (baked in by CMake). Placement in: the anchor pose from ARCore (Engine.cpp, ARTrackingState).
// Out: a list of Renderer::MeshDraw items each frame (the Renderer does the GL work), and the selected piece for the
// action buttons (Engine.cpp, GetActionsText).
//
// Spaces: a piece's mesh is in its own "local" space; Equipment.json places it relative to the anchor; the anchor pose
// places that in the ARCore world. Row vectors, so local * localModel * anchorPose = world (Matrix.hpp).
class EquipmentScene
{
public:
    static EquipmentScene& Get(); // the one scene (created the first time this is called)

    EquipmentScene(EquipmentScene const&)            = delete; // one scene only: no copies
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

    // Once per frame, last: the "looking for the card" loader, a ring of see-through dots spinning in the middle of the
    // screen. Draw it whenever the station is not shown. 'seconds' keeps growing (it drives the spin).
    void DrawLoader(float seconds, int viewportW, int viewportH);

    // false = the station is hidden (tracking cannot be trusted, or the card is covered): nothing is drawn or picked.
    void SetStationShown(bool shown) { stationShown = shown; }

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
    EquipmentLibrary const&    GetLibrary() const { return library; }
    std::optional<std::size_t> GetSelected() const { return selected; }

private:
    EquipmentScene() = default; // only Get() makes one

    static MeshData MakeMarkerMesh(); // a small flat square, lying in the x-z plane (debug square, planes, loader dots)

    void RefreshModels(); // models[i] = localModels[i] * anchorPose

    static char const* FindAsset(char const* path); // an embedded file's text by repo path; nullptr on desktop / missing
    static Vector3D    ColorFor(std::size_t index); // placeholder colour per piece until the data carries colours

    EquipmentLibrary           library;     // the pieces, sorted by id (from the ECS)
    std::vector<MeshData>      meshes;      // same order as library.items
    std::vector<Matrix4x4>     localModels; // local -> anchor-local (scale, yaw, position from the JSON), same order
    std::vector<Matrix4x4>     models;      // local -> ARCore world = localModels * anchorPose, same order
    Matrix4x4                  anchorPose;  // set by SetAnchorPose
    std::optional<std::size_t> selected;    // index into library.items of the tapped piece, if any
    std::vector<Matrix4x4>     planeModels; // each tracked plane as a scaled rectangle, local -> ARCore world
    int                        markerState = 0; // see SetMarker
    Matrix4x4                  markerPose;      // where the debug square lies
    bool                       loaded = false;  // Init succeeded: meshes read and handed to the Renderer
    bool                       placed = false;  // an anchor pose has arrived at least once
    bool                       stationShown = false; // see SetStationShown
    std::vector<Renderer::MeshDraw> draws;           // sandbox optimisation: this frame's draw list, reused (no new heap memory each frame)
};