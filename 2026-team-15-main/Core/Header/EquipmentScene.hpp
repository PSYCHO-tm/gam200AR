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

    // Once per GL context. Reads the Equipment.json and the meshes that CMake baked into the app.
    // Returns false (and logs why) if anything is missing or invalid.
    bool Init();

    // Once per frame. 'view' and 'projection' are the camera the player is looking through (the last frame drawn),
    // so a tap selects exactly what is on screen. 'tracking' is false while ARCore cannot place anything.
    void Update(float dt, InputSnapshot const& input, bool tracking, Matrix4x4 const& view, Matrix4x4 const& projection,
                int viewportW, int viewportH);

    // Once per frame, after the camera picture. Does nothing until the equipment has been placed.
    void Draw(Matrix4x4 const& view, Matrix4x4 const& projection);

    // For the buttons and text that will be added later: the equipment, and which piece (if any) is selected.
    EquipmentLibrary const&    GetLibrary() const { return library; }
    std::optional<std::size_t> GetSelected() const { return selected; }

private:
    EquipmentScene() = default;

    static char const* FindAsset(char const* path);
    static Vector3D    ColorFor(std::size_t index);

    EquipmentLibrary           library;
    std::vector<MeshData>      meshes; // same order as library.items
    std::vector<Matrix4x4>     models; // local -> ARCore world, same order; filled when the equipment is placed
    std::optional<std::size_t> selected;
    bool                       loaded = false;
    bool                       placed = false;
};