#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "Collider.hpp"
#include "equipmentDef.hpp"
#include "Matrix.hpp"
#include "Raycast.hpp"

// Which piece of equipment does a touch hit? Pure maths: no GL, no Android, so a host test can check it.
// (This file used to hold a Sim:: version that was fully commented out; Collider + Raycast replaced it.)

struct PickHit
{
    std::size_t index    = 0;    // position in the list that was given to PickNearest
    float       distance = 0.0f; // how far along the ray the box was entered
};

// 'worldBounds' are WORLD-space boxes (Collider::Transformed(model)), one per piece, in any fixed order.
// Returns the nearest box the ray enters, or nothing. Boxes without bounds are skipped.
std::optional<PickHit> PickNearest(Ray const& ray, std::vector<Collider> const& worldBounds);

// The same, for the equipment library: 'models[i]' is piece i's local -> world matrix
// (EquipmentLocalMatrix(def) * anchorPose). The hit's 'index' is the position in library.items.
std::optional<PickHit> PickEquipment(Ray const& ray, EquipmentLibrary const& library,
                                     std::vector<Matrix4x4> const& models);
