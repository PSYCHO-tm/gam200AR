#pragma once

// Equipment data (M1G05): what each piece of kit is, where it sits on the anchor, and what the player can do with it.
// Loaded from Assets/Json/Equipment.json. Positions are anchor-local, in meters.

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "sim/collider/aabb.hpp"
#include "sim/math/vectors.hpp"

namespace Sim
{
	struct InteractionDef
	{
		std::string Action;     // id the assessment core understands, e.g. "pour_water"
		std::string Label;      // text for the button
		std::string Ingredient; // ingredient id from Ingredients.json, e.g. "water"
		float       Quantity = 0.0f;
		std::string Units;
	};

	struct EquipmentDef
	{
		std::string                 Id; // key in the JSON, e.g. "Kettle"
		std::string                 Name;
		std::string                 MeshPath; // relative to the repo root
		Vec3                        Position; // anchor-local meters
		float                       YawDeg = 0.0f;
		float                       Scale  = 1.0f;
		std::vector<InteractionDef> Interactions;
		Aabb                        LocalBounds; // filled from the mesh by AttachMeshBounds
		bool                        HasBounds = false;
	};

	struct EquipmentLibrary
	{
		std::vector<EquipmentDef> Items; // JSON key order is not preserved; sorted by Id for stable output
	};

	// sourceName is only used in error messages. Errors name the equipment and the field.
	bool ParseEquipment(std::string const& text, std::string const& sourceName, EquipmentLibrary& library,
	                    std::string& error);
	bool LoadEquipmentFile(std::string const& path, EquipmentLibrary& library, std::string& error);

	// Loads each mesh (paths are relative to rootDir) and stores its bounds. Fails with the path in the message.
	bool AttachMeshBounds(EquipmentLibrary& library, std::string const& rootDir, std::string& error);

	// Same as above, but the text of each mesh file comes from 'readText' (path from the JSON -> file text, or nullopt).
	// Android uses this to read OBJs out of the APK's assets, where there is no disk path to open.
	using TextReader = std::function<std::optional<std::string>(std::string const&)>;
	bool AttachMeshBounds(EquipmentLibrary& library, TextReader const& readText, std::string& error);

	EquipmentDef const* FindEquipment(EquipmentLibrary const& library, std::string const& id);
}
