// componentRegistry.hpp - maps component names in data files to load/save code.
// To add a component: define the struct in components.hpp, then add one Register call
// in RegisterBuiltinComponents. No other code needs to change.
#pragma once
#include "Entity Component System/Ecs.hpp"
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Core
{
	using LoadFn = std::function<bool(Registry&, Entity, const nlohmann::json&, std::string&)>;
	// Returns false when the entity does not have the component.
	using SaveFn = std::function<bool(const Registry&, Entity, nlohmann::json&)>;

	struct ComponentInfo
	{
		std::string Name;
		LoadFn      Load;
		SaveFn      Save;
	};

	class ComponentRegistry
	{
	public:
		void                              Register(ComponentInfo info);
		const ComponentInfo*              Find(const std::string& name) const;
		const std::vector<ComponentInfo>& All() const { return Infos; }

	private:
		std::vector<ComponentInfo> Infos;
	};

	void RegisterBuiltinComponents(ComponentRegistry& registry);
}
