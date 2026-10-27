/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

SimData = the "filing cabinet". It reads the JSON files and turns them into the plain
structs from simTypes.hpp. It is the ONLY place that knows about JSON, so the rest of the
simulation never touches a file.

====================================================================================== */

#pragma once

#include "Simulation/simTypes.hpp"

#include <map>
#include <string>
#include <vector>

namespace core
{
	class SimData
	{
	public:
		std::vector<DrinkDef>                         drinks;
		std::vector<IngredientDef>                    ingredients;
		std::map<std::string, RecipeDef>              recipes;   // by recipe id
		std::map<std::string, ScenarioDef>            scenarios; // by scenario name ("Normal")
		std::map<std::string, std::vector<GradeBand>> grading;   // by grading id ("Default"), best grade first
		SimConfig                                     config;

		// ---- Load everything from a folder (desktop / Emscripten) ----------------------------
		// Needs Drinks, Ingredients, Recipes, Scenarios, Grading. Simulation.json is optional (defaults are used).
		// Returns false and sets GetLastError() if anything is missing, broken, or points at something that does not
		// exist.
		bool LoadFromDirectory(const std::string& folder);

		// ---- Or load one file's text at a time (Android: read the asset yourself, then pass the text) ----
		bool LoadDrinks(const std::string& jsonText);
		bool LoadIngredients(const std::string& jsonText);
		bool LoadRecipes(const std::string& jsonText);
		bool LoadScenarios(const std::string& jsonText);
		bool LoadGrading(const std::string& jsonText);
		bool LoadConfig(const std::string& jsonText);

		// Checks that the files agree with each other (every drink has a recipe, every recipe ingredient exists...)
		bool Validate();

		const DrinkDef*      FindDrink(const std::string& id) const;
		const IngredientDef* FindIngredient(const std::string& id) const;
		const RecipeDef*     FindRecipe(const std::string& id) const;
		const std::string&   GetLastError() const { return lastError; }

	private:
		std::string lastError;
	};
}
