#include "Entity Component System/ComponentRegistry.hpp"

#include "Entity Component System/Components.hpp"
#include <algorithm>
#include <initializer_list>
#include <string_view>

namespace Core
{
	using Json = nlohmann::json;

	namespace
	{
		bool CheckKeys(const Json& json, std::initializer_list<std::string_view> allowed, const std::string& comp,
		               std::string& error)
		{
			if (!json.is_object())
			{
				error = comp + ": expected an object";
				return false;
			}
			for (const auto& item : json.items())
			{
				const bool known = std::any_of(allowed.begin(), allowed.end(),
				                               [&](std::string_view key) { return key == item.key(); });
				if (!known)
				{
					error = comp + ": unknown field '" + item.key() + "'";
					return false;
				}
			}
			return true;
		}

		// Optional field: leaves out untouched when the key is absent.
		bool ReadVec3(const Json& json, const char* key, Vec3& out, const std::string& comp, std::string& error)
		{
			if (!json.contains(key))
				return true;
			const Json& arr = json[key];
			if (!arr.is_array() || arr.size() != 3 || !arr[0].is_number() || !arr[1].is_number() || !arr[2].is_number())
			{
				error = comp + "." + key + ": expected an array of 3 numbers";
				return false;
			}
			out = Vec3{arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
			return true;
		}

		bool ReadFloat(const Json& json, const char* key, float& out, const std::string& comp, std::string& error)
		{
			if (!json.contains(key))
				return true;
			if (!json[key].is_number())
			{
				error = comp + "." + key + ": expected a number";
				return false;
			}
			out = json[key].get<float>();
			return true;
		}

		bool ReadRequiredString(const Json& json, const char* key, std::string& out, const std::string& comp,
		                        std::string& error)
		{
			if (!json.contains(key) || !json[key].is_string())
			{
				error = comp + "." + key + ": required, expected a string";
				return false;
			}
			out = json[key].get<std::string>();
			return true;
		}

		Json Vec3ToJson(const Vec3& v) { return Json::array({v.x, v.y, v.z}); }
	}

	void ComponentRegistry::Register(ComponentInfo info) { Infos.push_back(std::move(info)); }

	const ComponentInfo* ComponentRegistry::Find(const std::string& name) const
	{
		for (const ComponentInfo& info : Infos)
			if (info.Name == name)
				return &info;
		return nullptr;
	}

	void RegisterBuiltinComponents(ComponentRegistry& registry)
	{
		registry.Register({"Name",
		                   [](Registry& reg, Entity entity, const Json& json, std::string& error)
		                   {
							   Name value;
							   if (!CheckKeys(json, {"value"}, "Name", error))
								   return false;
							   if (!ReadRequiredString(json, "value", value.Value, "Name", error))
								   return false;
							   reg.Add(entity, value);
							   return true;
						   },
		                   [](const Registry& reg, Entity entity, Json& out)
		                   {
							   const Name* value = reg.TryGet<Name>(entity);
							   if (value == nullptr)
								   return false;
							   out = Json{{"value", value->Value}};
							   return true;
						   }});

		registry.Register({"Transform",
		                   [](Registry& reg, Entity entity, const Json& json, std::string& error)
		                   {
							   Transform value;
							   if (!CheckKeys(json, {"position", "rotationDeg", "scale"}, "Transform", error))
								   return false;
							   if (!ReadVec3(json, "position", value.Position, "Transform", error))
								   return false;
							   if (!ReadVec3(json, "rotationDeg", value.Rotation, "Transform", error))
								   return false;
							   if (!ReadVec3(json, "scale", value.Scale, "Transform", error))
								   return false;
							   value.PrevRotation = value.Rotation;
							   value.PrevPosition = value.Position;
							   value.PrevScale    = value.Scale;
							   reg.Add(entity, value);
							   return true;
						   },
		                   [](const Registry& reg, Entity entity, Json& out)
		                   {
							   const Transform* value = reg.TryGet<Transform>(entity);
							   if (value == nullptr)
								   return false;
							   out = Json{{"position", Vec3ToJson(value->Position)},
							              {"rotationDeg", Vec3ToJson(value->Rotation)},
							              {"scale", Vec3ToJson(value->Scale)}};
							   return true;
						   }});

		registry.Register({"MeshRef",
		                   [](Registry& reg, Entity entity, const Json& json, std::string& error)
		                   {
							   MeshRef value;
							   if (!CheckKeys(json, {"path"}, "MeshRef", error))
								   return false;
							   if (!ReadRequiredString(json, "path", value.Path, "MeshRef", error))
								   return false;
							   reg.Add(entity, value);
							   return true;
						   },
		                   [](const Registry& reg, Entity entity, Json& out)
		                   {
							   const MeshRef* value = reg.TryGet<MeshRef>(entity);
							   if (value == nullptr)
								   return false;
							   out = Json{{"path", value->Path}};
							   return true;
						   }});

		registry.Register({"Material",
		                   [](Registry& reg, Entity entity, const Json& json, std::string& error)
		                   {
							   Material value;
							   if (!CheckKeys(json, {"color"}, "Material", error))
								   return false;
							   if (!ReadVec3(json, "color", value.Color, "Material", error))
								   return false;
							   reg.Add(entity, value);
							   return true;
						   },
		                   [](const Registry& reg, Entity entity, Json& out)
		                   {
							   const Material* value = reg.TryGet<Material>(entity);
							   if (value == nullptr)
								   return false;
							   out = Json{{"color", Vec3ToJson(value->Color)}};
							   return true;
						   }});

		registry.Register({"Spinner",
		                   [](Registry& reg, Entity entity, const Json& json, std::string& error)
		                   {
							   Spinner value;
							   if (!CheckKeys(json, {"degreesPerSecond"}, "Spinner", error))
								   return false;
							   if (!ReadFloat(json, "degreesPerSecond", value.DegreesPerSecond, "Spinner", error))
								   return false;
							   reg.Add(entity, value);
							   return true;
						   },
		                   [](const Registry& reg, Entity entity, Json& out)
		                   {
							   const Spinner* value = reg.TryGet<Spinner>(entity);
							   if (value == nullptr)
								   return false;
							   out = Json{{"degreesPerSecond", value->DegreesPerSecond}};
							   return true;
						   }});
	}
}
