#include "Entity Component System/SceneData.hpp"

#include "Entity Component System/Components.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace Core
{
	using Json = nlohmann::json;

	namespace
	{
		bool ParseJson(const std::string& text, const std::string& sourceName, Json& out, std::string& error)
		{
			out = Json::parse(text, nullptr, false);
			if (out.is_discarded())
			{
				error = sourceName + ": invalid JSON";
				return false;
			}
			return true;
		}

		bool BuildEntity(const Json& components, const ComponentRegistry& registry, Registry& out,
		                 const std::string& context, std::string& error)
		{
			if (!components.is_object())
			{
				error = context + ": 'components' must be an object";
				return false;
			}
			const Entity entity = out.Create();
			for (const auto& item : components.items())
			{
				const ComponentInfo* info = registry.Find(item.key());
				if (info == nullptr)
				{
					error = context + ": unknown component '" + item.key() + "'";
					return false;
				}
				std::string inner;
				if (!info->Load(out, entity, item.value(), inner))
				{
					error = context + ": " + inner;
					return false;
				}
			}
			return true;
		}
	}

	bool ReadTextFile(const std::string& path, std::string& out)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
			return false;
		std::stringstream buffer;
		buffer << file.rdbuf();
		out = buffer.str();
		return true;
	}

	bool ParseObjectDef(const std::string& text, const std::string& sourceName, ObjectDefs& defs, std::string& error)
	{
		Json json;
		if (!ParseJson(text, sourceName, json, error))
			return false;
		if (!json.is_object() || !json.contains("type") || !json["type"].is_string() ||
		    json["type"].get<std::string>().empty())
		{
			error = sourceName + ": 'type' is required and must be a non-empty string";
			return false;
		}
		if (!json.contains("components") || !json["components"].is_object())
		{
			error = sourceName + ": 'components' is required and must be an object";
			return false;
		}
		ObjectDef def;
		def.Type       = json["type"].get<std::string>();
		def.Components = json["components"];
		if (defs.count(def.Type) != 0)
		{
			error = sourceName + ": duplicate object type '" + def.Type + "'";
			return false;
		}
		defs[def.Type] = std::move(def);
		return true;
	}

	bool LoadObjectDefsFromDir(const std::string& dir, ObjectDefs& defs, std::string& error)
	{
		namespace Fs = std::filesystem;
		std::error_code code;
		if (!Fs::is_directory(dir, code))
		{
			error = "object definition folder not found: " + dir;
			return false;
		}
		std::vector<Fs::path> files;
		for (const auto& entry : Fs::directory_iterator(dir, code))
			if (entry.is_regular_file() && entry.path().extension() == ".json")
				files.push_back(entry.path());
		std::sort(files.begin(), files.end());

		for (const Fs::path& file : files)
		{
			std::string text;
			if (!ReadTextFile(file.string(), text))
			{
				error = "cannot read " + file.string();
				return false;
			}
			if (!ParseObjectDef(text, file.filename().string(), defs, error))
				return false;
		}
		return true;
	}

	bool InstantiateScene(const Json& scene, const ObjectDefs& defs, const ComponentRegistry& components, Registry& out,
	                      std::string& error)
	{
		if (!scene.is_object())
		{
			error = "scene: expected an object";
			return false;
		}

		if (scene.contains("instances"))
		{
			if (!scene["instances"].is_array())
			{
				error = "scene.instances: expected an array";
				return false;
			}
			std::size_t index = 0;
			for (const Json& inst : scene["instances"])
			{
				const std::string context = "scene.instances[" + std::to_string(index++) + "]";
				if (!inst.is_object() || !inst.contains("type") || !inst["type"].is_string())
				{
					error = context + ": 'type' is required and must be a string";
					return false;
				}
				const std::string type = inst["type"].get<std::string>();
				const auto        def  = defs.find(type);
				if (def == defs.end())
				{
					error = context + ": unknown object type '" + type + "'";
					return false;
				}
				Json merged = def->second.Components;
				if (inst.contains("overrides"))
				{
					if (!inst["overrides"].is_object())
					{
						error = context + ": 'overrides' must be an object";
						return false;
					}
					for (const auto& over : inst["overrides"].items())
					{
						if (over.value().is_object() && merged.contains(over.key()) && merged[over.key()].is_object())
							for (const auto& field : over.value().items())
								merged[over.key()][field.key()] = field.value();
						else
							merged[over.key()] = over.value();
					}
				}
				if (inst.contains("name"))
				{
					if (!inst["name"].is_string())
					{
						error = context + ": 'name' must be a string";
						return false;
					}
					merged["Name"] = Json{{"value", inst["name"]}};
				}
				if (!BuildEntity(merged, components, out, context + " (" + type + ")", error))
					return false;
			}
		}

		if (scene.contains("entities"))
		{
			if (!scene["entities"].is_array())
			{
				error = "scene.entities: expected an array";
				return false;
			}
			std::size_t index = 0;
			for (const Json& ent : scene["entities"])
			{
				const std::string context = "scene.entities[" + std::to_string(index++) + "]";
				if (!ent.is_object() || !ent.contains("components"))
				{
					error = context + ": 'components' is required";
					return false;
				}
				if (!BuildEntity(ent["components"], components, out, context, error))
					return false;
			}
		}
		return true;
	}

	bool InstantiateSceneText(const std::string& text, const ObjectDefs& defs, const ComponentRegistry& components,
	                          Registry& out, std::string& error)
	{
		Json json;
		if (!ParseJson(text, "scene", json, error))
			return false;
		return InstantiateScene(json, defs, components, out, error);
	}

	Json SerializeScene(const Registry& registry, const ComponentRegistry& components)
	{
		Json entities = Json::array();
		for (const Entity entity : registry.GetEntities())
		{
			Json comps = Json::object();
			for (const ComponentInfo& info : components.All())
			{
				Json value;
				if (info.Save(registry, entity, value))
					comps[info.Name] = value;
			}
			entities.push_back(Json{{"components", comps}});
		}
		return Json{{"entities", entities}};
	}
}
