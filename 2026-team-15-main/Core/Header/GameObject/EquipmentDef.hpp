#pragma once

#include <string>
#include <vector>

#include "../Collider/Aabb.hpp"
#include "../Math/Vec3.hpp"

namespace Core
{
	// One mapping from a native UI button (or gesture) to an action on a piece of equipment.
	struct InteractionDef
	{
		std::string Action; // e.g. "pour"
		std::string Button; // id of the native Android button that triggers it
		float       Quantity = 0.0f;
		std::string Unit;
	};

	struct EquipmentDef
	{
		std::string                 Id;
		std::string                 MeshPath; // relative to the project root
		Vec3                        Position; // offset from the anchor, metres
		float                       Scale = 1.0f;
		Vec3                        Color = {0.8f, 0.8f, 0.8f};
		std::vector<InteractionDef> Interactions;
		Aabb                        Bounds; // local mesh bounds, filled in by whoever loads the mesh
	};

	// Parses Assets/Json/Equipment.json. On failure returns false and outError names the object and field.
	bool LoadEquipmentDefs(std::string const& jsonText, std::vector<EquipmentDef>& outDefs, std::string& outError);
}
