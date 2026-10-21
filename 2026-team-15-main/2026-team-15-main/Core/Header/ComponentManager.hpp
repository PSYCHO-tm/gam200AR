#pragma once
#include "sharedlib.hpp"
#include "ComponentArray.hpp"

struct Name      { std::string Value; };
struct RecipeRef { std::string Id; };
struct Temp      { std::string Status; };

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Name, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RecipeRef, Id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Temp, Status)
namespace Ecs{
/*
	Components are plain data STRUCTS. Contains variables but no logic / functions
	Job is to initalize them with data based on ...

	Logic is kept in here because T (which will be of struct types) guarantees that compiler
	can read and deduce the full implementation.
*/
class ComponentManager
{
public:
	/*
		Checks for T type at compile-time. While mapping it into the maps if 
		its the first time its been registered
	*/
	template<typename T>
	void RegisterComponent();

	/*
		Returns index of recorded Component Type in map at compile time.
	*/
	template<typename T>
	ComponentType GetComponentType() const;

	/*
		Insert a component from the array for an entity
	*/
	template<typename T>
	void AddComponent(Entity entity, T component);

	/*
		Remove a component from the array for an entity
	*/
	template<typename T>
	void RemoveComponent(Entity entity);

	/*
		Get a reference to a component from the array for an entity
	*/
	template<typename T>
	T& GetComponent(Entity entity);

	/*
		Get a reference to a component from the array for an entity
	*/
	template<typename T>
	T const& GetComponent(Entity entity) const;

	/*
		Calls EntityDestroyed from ComponentArray to Destroy specific entity
	*/
	void EntityDestroyed(Entity entity);

	/*
		Helper function to check if type exists in maps
	*/
	template <typename T>
	bool HasComponent(Entity entity) const;

	/*
		Returns count of mNextComponentType
	*/
	ComponentType ComponentTypeCount() const;


private:
	// Maps component types to an ID
	std::unordered_map<std::string, ComponentType> mComponentTypes{};

	// Maps component type to an array [Double Mapping]
	std::unordered_map<std::string, std::shared_ptr<IComponentArray>> mComponentArrays{};

	ComponentType mNextComponentType{};

	// Convenience function to get the statically casted pointer to the ComponentArray of type T.
	template<typename T>
	std::shared_ptr<ComponentArray<T>> GetComponentArray()
	{
		std::string typeName = typeid(T).name();

		auto pos = mComponentArrays.find(typeName);
		assert(pos != mComponentArrays.end() && " [GetComponentArray] Component not registered before use.");

		return std::static_pointer_cast<ComponentArray<T>>(pos->second);
	}

	/*
		Const version
	*/
	template<typename T>
	std::shared_ptr<ComponentArray<T>> GetComponentArray() const
	{
		std::string typeName = typeid(T).name();

		auto pos = mComponentArrays.find(typeName);
		assert(pos != mComponentArrays.end() && " [GetComponentArray] Component not registered before use.");

		return std::static_pointer_cast<ComponentArray<T>>(pos->second);
	}
};
}

//================================================TEMPLATE CLASS DEFINITIONS====================================================

namespace Ecs{
	
	/*
		Checks for T type at compile-time. While mapping it into the maps if 
		its the first time its been registered
	*/
	template<typename T>
	void ComponentManager::RegisterComponent()
	{
		std::string typeName = typeid(T).name();

		assert(mComponentTypes.find(typeName) == mComponentTypes.end() && " [RegisterComponent] Registering component type more than once.");
		assert(mNextComponentType < MAX_COMPONENTS && " [RegisterComponent] Exceeded max amount of components.");

		// Adding component type to a map that stores component types
		mComponentTypes.insert({typeName, mNextComponentType});

		// Create a ComponentArray pointer and add it to the component arrays map
		mComponentArrays.insert({typeName, std::make_shared<ComponentArray<T>>()});

		++mNextComponentType;
	}

	/*
		Returns index of recorded Component Type in map at compile time.
	*/
	template<typename T>
	ComponentType ComponentManager::GetComponentType() const
	{
		std::string typeName = typeid(T).name();

		auto pos = mComponentTypes.find(typeName);
		assert(pos != mComponentTypes.end() && "[GetComponentType] Component not registered before use.");

		return pos->second;
	}

	/*
		Insert a component from the array for an entity
	*/
	template<typename T>
	void ComponentManager::AddComponent(Entity entity, T component)
	{
		GetComponentArray<T>()->InsertData(entity, std::move(component));
	}

	/*
		Remove a component from the array for an entity
	*/
	template<typename T>
	void ComponentManager::RemoveComponent(Entity entity)
	{	
		GetComponentArray<T>()->RemoveData(entity);
	}

	/*
		Get a reference to a component from the array for an entity
	*/
	template<typename T>
	T& ComponentManager::GetComponent(Entity entity)
	{
		return GetComponentArray<T>()->GetData(entity);
	}

	/*
		Get a reference to a component from the array for an entity
	*/
	template<typename T>
	T const& ComponentManager::GetComponent(Entity entity) const
	{
		return GetComponentArray<T>()->GetData(entity);
	}

	/*
		Helper function to check if type exists in maps
	*/
	template <typename T>
	bool ComponentManager::HasComponent(Entity entity) const{

		std::string typeName = typeid(T).name();
		auto pos = mComponentArrays.find(typeName);
        if ( pos == mComponentArrays.end()) {
            return false;
        }

       	return std::static_pointer_cast<ComponentArray<T>>(pos->second)->HasData(entity);
	}


	struct IngredientDef {
    std::string Name, Units;
    Json Visual, Audio;
    std::vector<std::string> State;       // optional
    std::vector<std::string> Sweetened;   // optional
	};

	struct RecipeIngredient { double Amount = 0; std::string State, Sweetened; };
	struct RecipeDef {
		std::unordered_map<std::string, RecipeIngredient> Ingredients;
		Json Visual, Audio;
	};

	struct DrinkDef    { std::string Name, Recipe, Status; Json Visual, Audio; };
	struct ScenarioDef { int Key = 0; double Time = 0; std::string Units; Json Visual, Audio; };
	struct GradeTier   { std::string Grade; double Score = 0; };

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(IngredientDef, Name, Units, Visual, Audio, State, Sweetened)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RecipeIngredient, Amount, State, Sweetened)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RecipeDef, Ingredients, Visual, Audio)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DrinkDef, Name, Recipe, Status, Visual, Audio)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScenarioDef, Key, Time, Units, Visual, Audio)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GradeTier, Grade, Score)
}