#pragma once
#include <cstddef>
#include <optional>
#include <vector>

#include "Geometry.hpp"
#include "Input.hpp"
#include "Matrix.hpp"
#include "EquipmentDef.hpp"

// The equipment you see in AR: loads it, keeps it still in the room, selects it when you tap it, and draws it.
// Everything runs on the GL thread, called from Core (Init / Update / Render), so nothing here needs a lock.
//
// There is exactly one EquipmentScene, reached through Get(). Nobody owns it.
class EquipmentScene
{
public:
    static EquipmentScene& Get();

    EquipmentScene(EquipmentScene const&)            = delete;
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
    EquipmentScene() = default;

    static MeshData MakeMarkerMesh(); // a small flat square, lying in the x-z plane

    void RefreshModels(); // models[i] = localModels[i] * anchorPose

    static char const* FindAsset(char const* path);
    static Vector3D    ColorFor(std::size_t index);

    EquipmentLibrary           library;
    std::vector<MeshData>      meshes;      // same order as library.items
    std::vector<Matrix4x4>     localModels; // local -> anchor-local (scale, yaw, position from the JSON), same order
    std::vector<Matrix4x4>     models;      // local -> ARCore world = localModels * anchorPose, same order
    Matrix4x4                  anchorPose;  // set by SetAnchorPose
    std::optional<std::size_t> selected;
    std::vector<Matrix4x4>     planeModels; // each tracked plane as a scaled rectangle, local -> ARCore world
    int                        markerState = 0;
    Matrix4x4                  markerPose;
    bool                       loaded = false;
    bool                       placed = false;
};