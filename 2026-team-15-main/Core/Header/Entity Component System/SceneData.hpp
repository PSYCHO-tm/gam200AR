// sceneData.hpp - data-driven objects and scenes (M1G02).
//
// Object definition file (data/objects/*.json):
//   { "type": "crate", "components": { "Transform": {...}, "MeshRef": {...} } }
// Scene file (data/scenes/*.json):
//   { "instances": [ { "type": "crate", "name": "crateA", "overrides": { "Transform": {...} } } ],
//     "entities":  [ { "components": { ... } } ] }       // "entities" = fully explicit (used by Save)
#pragma once
#include "Entity Component System/ComponentRegistry.hpp"
#include "Entity Component System/Ecs.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace Core
{
	struct ObjectDef
	{
		std::string    Type;
		nlohmann::json Components = nlohmann::json::object();
	};

	using ObjectDefs = std::map<std::string, ObjectDef>;

	bool ReadTextFile(const std::string& path, std::string& out);

	// sourceName is only used in error messages (usually the file name).
	bool ParseObjectDef(const std::string& text, const std::string& sourceName, ObjectDefs& defs, std::string& error);
	bool LoadObjectDefsFromDir(const std::string& dir, ObjectDefs& defs, std::string& error);

	// Builds entities into 'out'. On failure 'out' may be partly filled: callers discard it.
	bool InstantiateScene(const nlohmann::json& scene, const ObjectDefs& defs, const ComponentRegistry& components,
	                      Registry& out, std::string& error);
	bool InstantiateSceneText(const std::string& text, const ObjectDefs& defs, const ComponentRegistry& components,
	                          Registry& out, std::string& error);

	// Writes every entity with all of its registered components (deterministic order).
	nlohmann::json SerializeScene(const Registry& registry, const ComponentRegistry& components);
}
