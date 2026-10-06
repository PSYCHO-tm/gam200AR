#include "../../Header/GameObject/EquipmentDef.hpp"

#include <exception>

#include <nlohmann/json.hpp>

namespace Core
{
	namespace
	{
		using Json = nlohmann::ordered_json;

		bool ReadVec3(Json const& obj, char const* key, Vec3& out, std::string const& who, std::string& error)
		{
			if (!obj.contains(key))
			{
				return true; // optional, keep the default
			}
			Json const& arr = obj[key];
			if (!arr.is_array() || arr.size() != 3 || !arr[0].is_number() || !arr[1].is_number() || !arr[2].is_number())
			{
				error = who + ": '" + key + "' must be an array of 3 numbers";
				return false;
			}
			out = {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
			return true;
		}

		bool ReadInteraction(Json const& obj, std::string const& who, InteractionDef& out, std::string& error)
		{
			if (!obj.is_object())
			{
				error = who + ": each interaction must be an object";
				return false;
			}
			if (!obj.contains("Action") || !obj["Action"].is_string())
			{
				error = who + ": interaction is missing string field 'Action'";
				return false;
			}
			if (!obj.contains("Button") || !obj["Button"].is_string())
			{
				error =
					who + ": interaction '" + obj["Action"].get<std::string>() + "' is missing string field 'Button'";
				return false;
			}
			out.Action = obj["Action"].get<std::string>();
			out.Button = obj["Button"].get<std::string>();
			if (obj.contains("Quantity"))
			{
				if (!obj["Quantity"].is_number() || obj["Quantity"].get<float>() < 0.0f)
				{
					error = who + ": interaction '" + out.Action + "' has a non-numeric or negative 'Quantity'";
					return false;
				}
				out.Quantity = obj["Quantity"].get<float>();
			}
			if (obj.contains("Unit") && obj["Unit"].is_string())
			{
				out.Unit = obj["Unit"].get<std::string>();
			}
			return true;
		}
	}

	bool LoadEquipmentDefs(std::string const& jsonText, std::vector<EquipmentDef>& outDefs, std::string& outError)
	{
		try
		{
			Json const root = Json::parse(jsonText);
			if (!root.is_object() || !root.contains("EQUIPMENT") || !root["EQUIPMENT"].is_object())
			{
				outError = "Equipment file needs a top-level object named 'EQUIPMENT'";
				return false;
			}

			std::vector<EquipmentDef> defs;
			for (auto const& item : root["EQUIPMENT"].items())
			{
				std::string const who = "Equipment '" + item.key() + "'";
				Json const&       obj = item.value();
				if (!obj.is_object())
				{
					outError = who + " must be an object";
					return false;
				}

				EquipmentDef def;
				def.Id = item.key();
				if (!obj.contains("Mesh") || !obj["Mesh"].is_string())
				{
					outError = who + ": missing required string field 'Mesh'";
					return false;
				}
				def.MeshPath = obj["Mesh"].get<std::string>();

				if (!ReadVec3(obj, "Position", def.Position, who, outError) ||
				    !ReadVec3(obj, "Color", def.Color, who, outError))
				{
					return false;
				}
				if (obj.contains("Scale"))
				{
					if (!obj["Scale"].is_number() || obj["Scale"].get<float>() <= 0.0f)
					{
						outError = who + ": 'Scale' must be a number greater than 0";
						return false;
					}
					def.Scale = obj["Scale"].get<float>();
				}
				if (obj.contains("Interactions"))
				{
					if (!obj["Interactions"].is_array())
					{
						outError = who + ": 'Interactions' must be an array";
						return false;
					}
					for (Json const& entry : obj["Interactions"])
					{
						InteractionDef interaction;
						if (!ReadInteraction(entry, who, interaction, outError))
						{
							return false;
						}
						def.Interactions.push_back(interaction);
					}
				}
				defs.push_back(def);
			}
			if (defs.empty())
			{
				outError = "Equipment file defines no equipment";
				return false;
			}
			outDefs = defs;
			return true;
		}
		catch (std::exception const& e)
		{
			outError = std::string("Equipment JSON error: ") + e.what();
			return false;
		}
	}
}
