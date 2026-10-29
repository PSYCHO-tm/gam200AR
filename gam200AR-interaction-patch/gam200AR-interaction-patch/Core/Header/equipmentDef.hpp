#pragma once

// Equipment data (M1G05): what each piece of kit is, where it sits on the anchor, and what the player can do with it.
// Loaded from Assets/Json/Equipment.json. Positions are anchor-local, in meters.

#include <map>
#include <string>
#include <vector>

#include "Collider.hpp"
#include "Matrix.hpp"
#include "Vector.hpp"

struct InteractionDef
{
    std::string action;     // id the assessment core understands, e.g. "pour_water"
    std::string label;      // text for the button
    std::string ingredient; // ingredient id from Ingredients.json, e.g. "water"
    float       quantity = 0.0f;
    std::string units;
    // What the assessment core compares against the recipe, e.g. {"State": "ground"}. Optional.
    std::map<std::string, std::string> attributes;
    // How the assessment core sees this interaction: "add", "serve" or "refill". Optional.
    // Empty = "add" when 'ingredient' is set, otherwise the action id itself.
    std::string assessAs;
};

struct EquipmentDef
{
    std::string                 id; // key in the JSON, e.g. "Kettle"
    std::string                 name;
    std::string                 meshPath; // relative to the repo root, e.g. "Assets/Models/kettle.obj"
    Vector3D                    position{0.0f, 0.0f, 0.0f}; // anchor-local meters
    float                       yawDeg = 0.0f;
    float                       scale  = 1.0f;
    std::vector<InteractionDef> interactions;
    Collider                    localBounds; // the mesh's box, filled in by whoever loads the mesh
};

// Where a piece sits on the anchor: scale, then yaw, then its position (row vectors, leftmost applies first).
// Multiply by the anchor's pose to get local -> ARCore world.
Matrix4x4 EquipmentLocalMatrix(EquipmentDef const& def);

class EquipmentLibrary
{
public:
    std::vector<EquipmentDef> items; // JSON key order is not preserved; sorted by id for stable output

    // sourceName is only used in error messages. Errors name the equipment and the field.
    // On failure 'items' is left untouched.
    bool Parse(std::string const& text, std::string const& sourceName, std::string& error);

    EquipmentDef const* Find(std::string const& id) const;
};
