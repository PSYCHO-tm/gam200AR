#include "sim/equipment/equipmentDef.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "json.hpp"
#include "sim/equipment/objLoader.hpp"

namespace Sim
{
	using Json = nlohmann::json;

	namespace
	{
		bool ReadVec3(Json const& json, std::string const& where, Vec3& out, std::string& error)
		{
			if (!json.is_array() || json.size() != 3 || !json[0].is_number() || !json[1].is_number() ||
			    !json[2].is_number())
			{
				error = where + ": expected an array of 3 numbers";
				return false;
			}
			out = {json[0].get<float>(), json[1].get<float>(), json[2].get<float>()};
			return true;
		}

		bool ReadString(Json const& json, char const* key, std::string const& where, std::string& out, bool required,
		                std::string& error)
		{
			if (!json.contains(key))
			{
				if (required)
				{
					error = where + "." + key + ": required";
				}
				return !required;
			}
			if (!json[key].is_string())
			{
				error = where + "." + key + ": expected a string";
				return false;
			}
			out = json[key].get<std::string>();
			return true;
		}

		bool ParseInteractions(Json const& json, std::string const& where, std::vector<InteractionDef>& out,
		                       std::string& error)
		{
			if (!json.is_object())
			{
				error = where + ": expected an object of action ids";
				return false;
			}
			for (auto const& item : json.items())
			{
				std::string const here = where + "." + item.key();
				Json const&       body = item.value();
				if (!body.is_object())
				{
					error = here + ": expected an object";
					return false;
				}
				InteractionDef def;
				def.Action = item.key();
				if (!ReadString(body, "Label", here, def.Label, false, error) ||
				    !ReadString(body, "Ingredient", here, def.Ingredient, false, error) ||
				    !ReadString(body, "Units", here, def.Units, false, error))
				{
					return false;
				}
				if (def.Label.empty())
				{
					def.Label = def.Action;
				}
				if (body.contains("Quantity"))
				{
					if (!body["Quantity"].is_number() || body["Quantity"].get<float>() < 0.0f)
					{
						error = here + ".Quantity: expected a number that is 0 or more";
						return false;
					}
					def.Quantity = body["Quantity"].get<float>();
				}
				out.push_back(def);
			}
			return true;
		}
	}

	bool ParseEquipment(std::string const& text, std::string const& sourceName, EquipmentLibrary& library,
	                    std::string& error)
	{
		Json const json = Json::parse(text, nullptr, false);
		if (json.is_discarded() || !json.is_object())
		{
			error = sourceName + ": invalid JSON";
			return false;
		}
		if (!json.contains("EQUIPMENT") || !json["EQUIPMENT"].is_object() || json["EQUIPMENT"].empty())
		{
			error = sourceName + ": 'EQUIPMENT' must be a non-empty object";
			return false;
		}

		EquipmentLibrary result;
		for (auto const& item : json["EQUIPMENT"].items())
		{
			std::string const where = sourceName + ": " + item.key();
			Json const&       body  = item.value();
			if (!body.is_object())
			{
				error = where + ": expected an object";
				return false;
			}
			EquipmentDef def;
			def.Id   = item.key();
			def.Name = item.key();
			if (!ReadString(body, "Name", where, def.Name, false, error) ||
			    !ReadString(body, "Mesh", where, def.MeshPath, true, error))
			{
				return false;
			}
			if (body.contains("Position") && !ReadVec3(body["Position"], where + ".Position", def.Position, error))
			{
				return false;
			}
			if (body.contains("YawDeg"))
			{
				if (!body["YawDeg"].is_number())
				{
					error = where + ".YawDeg: expected a number";
					return false;
				}
				def.YawDeg = body["YawDeg"].get<float>();
			}
			if (body.contains("Scale"))
			{
				if (!body["Scale"].is_number() || body["Scale"].get<float>() <= 0.0f)
				{
					error = where + ".Scale: expected a number greater than 0";
					return false;
				}
				def.Scale = body["Scale"].get<float>();
			}
			if (body.contains("Interactions") &&
			    !ParseInteractions(body["Interactions"], where + ".Interactions", def.Interactions, error))
			{
				return false;
			}
			result.Items.push_back(def);
		}
		std::sort(result.Items.begin(), result.Items.end(),
		          [](EquipmentDef const& a, EquipmentDef const& b) { return a.Id < b.Id; });
		library = std::move(result);
		return true;
	}

	bool LoadEquipmentFile(std::string const& path, EquipmentLibrary& library, std::string& error)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			error = "cannot open " + path;
			return false;
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		return ParseEquipment(buffer.str(), path, library, error);
	}

	bool AttachMeshBounds(EquipmentLibrary& library, std::string const& rootDir, std::string& error)
	{
		for (EquipmentDef& def : library.Items)
		{
			std::string const             path = rootDir.empty() ? def.MeshPath : rootDir + "/" + def.MeshPath;
			std::optional<MeshData> const mesh = LoadObj(path);
			if (!mesh)
			{
				error = def.Id + ": cannot load mesh '" + path + "'";
				return false;
			}
			def.LocalBounds = ComputeBounds(*mesh);
			def.HasBounds   = true;
		}
		return true;
	}

	bool AttachMeshBounds(EquipmentLibrary& library, TextReader const& readText, std::string& error)
	{
		for (EquipmentDef& def : library.Items)
		{
			std::optional<std::string> const text = readText(def.MeshPath);
			if (!text)
			{
				error = def.Id + ": cannot read mesh '" + def.MeshPath + "'";
				return false;
			}
			std::optional<MeshData> const mesh = ParseObj(*text);
			if (!mesh)
			{
				error = def.Id + ": mesh '" + def.MeshPath + "' has no usable geometry";
				return false;
			}
			def.LocalBounds = ComputeBounds(*mesh);
			def.HasBounds   = true;
		}
		return true;
	}

	EquipmentDef const* FindEquipment(EquipmentLibrary const& library, std::string const& id)
	{
		for (EquipmentDef const& def : library.Items)
		{
			if (def.Id == id)
			{
				return &def;
			}
		}
		return nullptr;
	}
}
