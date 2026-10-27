/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

====================================================================================== */

#include "Simulation/simData.hpp"

// json.hpp should live in Core/Header/nlohmann/ (that is where the ECS code looks for it).
// If it is still sitting in Assets/Json/ we fall back to that, so this file builds either way.
#if __has_include("nlohmann/json.hpp")
#	include "nlohmann/json.hpp"
#else
#	include "json.hpp"
#endif

#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <sstream>

namespace core
{
	namespace
	{
		// ordered_json keeps things in the same order as they appear in the file.
		// That matters: the ORDER of ingredients in Recipes.json is the order of the prep steps.
		using Json = nlohmann::ordered_json;

		// Walks down a path like {"Components", "Name", "Value"}. Returns nullptr if any part is missing.
		const Json* Dig(const Json* node, std::initializer_list<const char*> path)
		{
			for (const char* key : path)
			{
				if (node == nullptr || !node->is_object()) return nullptr;
				const auto it = node->find(key);
				if (it == node->end()) return nullptr;
				node = &(*it);
			}
			return node;
		}

		// parse(text, no callback, false) = "do NOT throw if the JSON is broken". We check is_discarded() instead.
		bool ParseJson(const std::string& text, const char* fileName, Json& out, std::string& error)
		{
			out = Json::parse(text, nullptr, false);
			if (out.is_discarded())
			{
				error = Fmt("%s: not valid JSON (check commas, quotes and brackets)", fileName);
				return false;
			}
			return true;
		}

		// Finds a top-level section like "DRINKS" and checks that it is an object
		const Json* Section(const Json& root, const char* name, const char* fileName, std::string& error)
		{
			const Json* section = Dig(&root, {name});
			if (section == nullptr || !section->is_object())
			{
				error = Fmt("%s: needs a top-level \"%s\" object", fileName, name);
				return nullptr;
			}
			return section;
		}

		// If node[key] exists it must be a number. Missing is fine (the default stays).
		bool ReadNumber(const Json& node, const char* key, float& out, const std::string& where, std::string& error)
		{
			const Json* value = Dig(&node, {key});
			if (value == nullptr) return true;
			if (!value->is_number())
			{
				error = Fmt("%s: \"%s\" must be a number", where.c_str(), key);
				return false;
			}
			out = value->get<float>();
			return true;
		}

		bool ReadText(const Json* node, std::string& out)
		{
			if (node == nullptr || !node->is_string()) return false;
			out = node->get<std::string>();
			return true;
		}

		// If node[key] exists it must be true or false. Missing is fine (the default stays).
		bool ReadBool(const Json& node, const char* key, bool& out, const std::string& where, std::string& error)
		{
			const Json* value = Dig(&node, {key});
			if (value == nullptr) return true;
			if (!value->is_boolean())
			{
				error = Fmt("%s: \"%s\" must be true or false", where.c_str(), key);
				return false;
			}
			out = value->get<bool>();
			return true;
		}

		bool ReadFile(const std::string& path, std::string& out)
		{
			std::ifstream file(path, std::ios::binary);
			if (!file) return false;
			std::stringstream buffer;
			buffer << file.rdbuf(); // read the whole file into one string
			out = buffer.str();
			return true;
		}
	}

	// ======================================================================================
	// Drinks.json   "Kopi_O": { "Components": { "Name": {"Value": "Kopi O"}, "RecipeRef": {"Id": ...}, "Temp":
	// {"Status": "Hot"} } }
	// ======================================================================================
	bool SimData::LoadDrinks(const std::string& jsonText)
	{
		Json root;
		if (!ParseJson(jsonText, "Drinks.json", root, lastError)) return false;
		const Json* section = Section(root, "DRINKS", "Drinks.json", lastError);
		if (section == nullptr) return false;

		std::vector<DrinkDef> loaded;
		for (const auto& [key, entry] : section->items())
		{
			DrinkDef    drink;
			std::string status;
			drink.id = key;

			if (!ReadText(Dig(&entry, {"Components", "Name", "Value"}), drink.displayName))
			{
				lastError = Fmt("Drinks.json: drink \"%s\" needs Components.Name.Value (text)", key.c_str());
				return false;
			}
			if (!ReadText(Dig(&entry, {"Components", "RecipeRef", "Id"}), drink.recipeId))
			{
				lastError = Fmt("Drinks.json: drink \"%s\" needs Components.RecipeRef.Id (text)", key.c_str());
				return false;
			}
			if (!ReadText(Dig(&entry, {"Components", "Temp", "Status"}), status))
			{
				lastError = Fmt("Drinks.json: drink \"%s\" needs Components.Temp.Status (text)", key.c_str());
				return false;
			}
			if (status == "Hot") drink.status = DrinkStatus::Hot;
			else if (status == "Iced")
				drink.status = DrinkStatus::Iced;
			else
			{
				lastError = Fmt("Drinks.json: drink \"%s\" has Status \"%s\", expected \"Hot\" or \"Iced\"",
				                key.c_str(), status.c_str());
				return false;
			}
			loaded.push_back(std::move(drink));
		}

		if (loaded.empty())
		{
			lastError = "Drinks.json: \"DRINKS\" is empty, a coffee shop needs at least one drink";
			return false;
		}
		drinks = std::move(loaded);
		lastError.clear();
		return true;
	}

	// ======================================================================================
	// Ingredients.json
	// ======================================================================================
	bool SimData::LoadIngredients(const std::string& jsonText)
	{
		Json root;
		if (!ParseJson(jsonText, "Ingredients.json", root, lastError)) return false;
		const Json* section = Section(root, "INGREDIENTS", "Ingredients.json", lastError);
		if (section == nullptr) return false;

		std::vector<IngredientDef> loaded;
		for (const auto& [key, entry] : section->items())
		{
			IngredientDef item;
			item.id = key;
			if (!ReadText(Dig(&entry, {"Components", "Name", "Value"}), item.displayName) ||
			    !ReadText(Dig(&entry, {"Components", "Units", "Value"}), item.units))
			{
				lastError =
					Fmt("Ingredients.json: \"%s\" needs Components.Name.Value and Components.Units.Value (text)",
				        key.c_str());
				return false;
			}
			loaded.push_back(std::move(item));
		}
		ingredients = std::move(loaded);
		lastError.clear();
		return true;
	}

	// ======================================================================================
	// Recipes.json   the ORDER of the ingredients is the order of the steps
	// ======================================================================================
	bool SimData::LoadRecipes(const std::string& jsonText)
	{
		Json root;
		if (!ParseJson(jsonText, "Recipes.json", root, lastError)) return false;
		const Json* section = Section(root, "RECIPES", "Recipes.json", lastError);
		if (section == nullptr) return false;

		std::map<std::string, RecipeDef> loaded;
		for (const auto& [recipeId, recipeNode] : section->items())
		{
			const Json* steps = Dig(&recipeNode, {"Ingredients"});
			if (steps == nullptr || !steps->is_object() || steps->empty())
			{
				lastError =
					Fmt("Recipes.json: recipe \"%s\" needs a non-empty \"Ingredients\" object", recipeId.c_str());
				return false;
			}

			RecipeDef recipe;
			recipe.id = recipeId;
			for (const auto& [ingredientId, spec] : steps->items())
			{
				const std::string where = Fmt("Recipes.json: %s.%s", recipeId.c_str(), ingredientId.c_str());

				RecipeStep step;
				step.ingredientId = ingredientId;

				const Json* amount = Dig(&spec, {"Amount"});
				if (amount == nullptr || !amount->is_number() || amount->get<float>() <= 0.0f)
				{
					lastError = where + ": needs \"Amount\" as a number above 0";
					return false;
				}
				step.amount = amount->get<float>();

				const Json* attributes = Dig(&spec, {"Attributes"});
				if (attributes != nullptr)
				{
					if (!attributes->is_object())
					{
						lastError = where + ": \"Attributes\" must be an object";
						return false;
					}
					for (const auto& [attrName, attrValue] : attributes->items())
					{
						if (!attrValue.is_string())
						{
							lastError = where + ": attribute \"" + attrName + "\" must be text";
							return false;
						}
						step.attributes[attrName] = attrValue.get<std::string>();
					}
				}
				recipe.steps.push_back(std::move(step));
			}
			loaded[recipeId] = std::move(recipe);
		}
		recipes = std::move(loaded);
		lastError.clear();
		return true;
	}

	// ======================================================================================
	// Scenarios.json
	// ======================================================================================
	bool SimData::LoadScenarios(const std::string& jsonText)
	{
		Json root;
		if (!ParseJson(jsonText, "Scenarios.json", root, lastError)) return false;
		const Json* section = Section(root, "SCENARIO", "Scenarios.json", lastError);
		if (section == nullptr) return false;

		std::map<std::string, ScenarioDef> loaded;
		for (const auto& [name, entry] : section->items())
		{
			const std::string where = "Scenarios.json: " + name;

			ScenarioDef scenario;
			scenario.name = name;

			float order = 0.0f;
			if (!ReadNumber(entry, "Order", order, where, lastError) ||
			    !ReadNumber(entry, "TimeLimitSec", scenario.timeLimitSec, where, lastError))
				return false;
			scenario.order = static_cast<int>(order);

			if (!ReadText(Dig(&entry, {"Grading"}), scenario.gradingId))
			{
				lastError = where + ": needs \"Grading\" (text, the id used in Grading.json)";
				return false;
			}

			const Json* orders = Dig(&entry, {"Orders"});
			if (orders == nullptr || !orders->is_array())
			{
				lastError = where + ": needs an \"Orders\" list";
				return false;
			}
			for (const auto& item : *orders)
			{
				ScenarioOrder so;
				float         count = 1.0f;
				if (!ReadText(Dig(&item, {"Drink"}), so.drinkId))
				{
					lastError = where + ": every entry in \"Orders\" needs \"Drink\" (text)";
					return false;
				}
				if (!ReadNumber(item, "Count", count, where, lastError)) return false;
				so.count = static_cast<int>(count);
				if (so.count < 1)
				{
					lastError = where + ": \"Count\" must be at least 1";
					return false;
				}
				scenario.orders.push_back(std::move(so));
			}
			loaded[name] = std::move(scenario);
		}
		scenarios = std::move(loaded);
		lastError.clear();
		return true;
	}

	// ======================================================================================
	// Grading.json
	// ======================================================================================
	bool SimData::LoadGrading(const std::string& jsonText)
	{
		Json root;
		if (!ParseJson(jsonText, "Grading.json", root, lastError)) return false;
		const Json* section = Section(root, "GRADING", "Grading.json", lastError);
		if (section == nullptr) return false;

		std::map<std::string, std::vector<GradeBand>> loaded;
		for (const auto& [id, list] : section->items())
		{
			if (!list.is_array() || list.empty())
			{
				lastError = Fmt("Grading.json: \"%s\" must be a non-empty list of { Grade, Score }", id.c_str());
				return false;
			}
			std::vector<GradeBand> bands;
			for (const auto& item : list)
			{
				GradeBand band;
				if (!ReadText(Dig(&item, {"Grade"}), band.grade) ||
				    !ReadNumber(item, "Score", band.minScore, id, lastError))
				{
					lastError =
						Fmt("Grading.json: \"%s\" entries need \"Grade\" (text) and \"Score\" (number)", id.c_str());
					return false;
				}
				bands.push_back(std::move(band));
			}
			// best grade first, so "the first band you reach" is your grade
			std::sort(bands.begin(), bands.end(),
			          [](const GradeBand& a, const GradeBand& b) { return a.minScore > b.minScore; });
			loaded[id] = std::move(bands);
		}
		grading = std::move(loaded);
		lastError.clear();
		return true;
	}

	// ======================================================================================
	// Simulation.json  (this file belongs to M1G06, every key is optional)
	// ======================================================================================
	bool SimData::LoadConfig(const std::string& jsonText)
	{
		Json root;
		if (!ParseJson(jsonText, "Simulation.json", root, lastError)) return false;
		const Json* section = Section(root, "SIMULATION", "Simulation.json", lastError);
		if (section == nullptr) return false;

		SimConfig loaded; // starts with the defaults from simTypes.hpp

		if (const Json* orders = Dig(section, {"Orders"}))
		{
			const std::string where    = "Simulation.json: Orders";
			float             maxQueue = static_cast<float>(loaded.maxQueueSize);
			float             seed     = static_cast<float>(loaded.seed);
			if (!ReadNumber(*orders, "InitialDelaySec", loaded.initialDelay, where, lastError) ||
			    !ReadNumber(*orders, "SpawnIntervalSec", loaded.spawnInterval, where, lastError) ||
			    !ReadNumber(*orders, "PatienceSec", loaded.orderLifetime, where, lastError) ||
			    !ReadNumber(*orders, "MaxQueue", maxQueue, where, lastError) ||
			    !ReadNumber(*orders, "Seed", seed, where, lastError))
				return false;
			if (loaded.spawnInterval <= 0.0f || loaded.orderLifetime <= 0.0f || maxQueue < 1.0f)
			{
				lastError = where + ": SpawnIntervalSec, PatienceSec and MaxQueue must be above 0";
				return false;
			}
			loaded.maxQueueSize = static_cast<std::size_t>(maxQueue);
			loaded.seed         = static_cast<unsigned>(seed);
			if (!ReadBool(*orders, "Repeat", loaded.repeatCustomers, where, lastError)) return false;
		}

		if (const Json* gradingNode = Dig(section, {"Grading"}))
		{
			if (!ReadNumber(*gradingNode, "QuantityTolerance", loaded.quantityTolerance, "Simulation.json: Grading",
			                lastError))
				return false;
			if (loaded.quantityTolerance < 0.0f)
			{
				lastError = "Simulation.json: Grading.QuantityTolerance cannot be negative";
				return false;
			}
		}

		if (const Json* inventory = Dig(section, {"Inventory"}))
		{
			if (!inventory->is_object())
			{
				lastError = "Simulation.json: \"Inventory\" must be an object";
				return false;
			}
			for (const auto& [id, rule] : inventory->items())
			{
				StockRule   stock;
				std::string where = "Simulation.json: Inventory." + id;
				if (!ReadNumber(rule, "Start", stock.start, where, lastError) ||
				    !ReadNumber(rule, "RefillAmount", stock.refillAmount, where, lastError) ||
				    !ReadNumber(rule, "LowThreshold", stock.lowThreshold, where, lastError))
					return false;
				loaded.stock[id] = stock;
			}
		}

		if (const Json* conditions = Dig(section, {"Conditions"}))
		{
			if (!conditions->is_array())
			{
				lastError = "Simulation.json: \"Conditions\" must be a list";
				return false;
			}
			for (const auto& item : *conditions)
			{
				Condition c;
				if (!ReadText(Dig(&item, {"Name"}), c.name))
				{
					lastError = "Simulation.json: every condition needs a \"Name\" (text)";
					return false;
				}
				const std::string where = "Simulation.json: condition " + c.name;
				if (!ReadNumber(item, "StartSec", c.startSec, where, lastError) ||
				    !ReadNumber(item, "DurationSec", c.durationSec, where, lastError) ||
				    !ReadNumber(item, "OrderRateMultiplier", c.orderRateMultiplier, where, lastError) ||
				    !ReadNumber(item, "PatienceMultiplier", c.patienceMultiplier, where, lastError))
					return false;
				if (c.orderRateMultiplier <= 0.0f || c.patienceMultiplier <= 0.0f)
				{
					lastError = where + ": multipliers must be above 0";
					return false;
				}
				loaded.conditions.push_back(std::move(c));
			}
		}

		config = std::move(loaded);
		lastError.clear();
		return true;
	}

	// ======================================================================================
	// Do the files agree with each other?
	// ======================================================================================
	bool SimData::Validate()
	{
		for (const DrinkDef& drink : drinks)
		{
			if (FindRecipe(drink.recipeId) == nullptr)
			{
				lastError = Fmt("Drink \"%s\" uses recipe \"%s\" which is not in Recipes.json", drink.id.c_str(),
				                drink.recipeId.c_str());
				return false;
			}
		}
		for (const auto& [recipeId, recipe] : recipes)
		{
			for (const RecipeStep& step : recipe.steps)
			{
				if (FindIngredient(step.ingredientId) == nullptr)
				{
					lastError = Fmt("Recipe \"%s\" uses ingredient \"%s\" which is not in Ingredients.json",
					                recipeId.c_str(), step.ingredientId.c_str());
					return false;
				}
			}
		}
		for (const auto& [name, scenario] : scenarios)
		{
			if (grading.find(scenario.gradingId) == grading.end())
			{
				lastError = Fmt("Scenario \"%s\" uses grading \"%s\" which is not in Grading.json", name.c_str(),
				                scenario.gradingId.c_str());
				return false;
			}
			for (const ScenarioOrder& so : scenario.orders)
			{
				if (FindDrink(so.drinkId) == nullptr)
				{
					lastError = Fmt("Scenario \"%s\" orders drink \"%s\" which is not in Drinks.json", name.c_str(),
					                so.drinkId.c_str());
					return false;
				}
			}
		}
		for (const auto& [id, rule] : config.stock)
		{
			(void)rule;
			if (FindIngredient(id) == nullptr)
			{
				lastError = Fmt("Simulation.json: Inventory has \"%s\" which is not in Ingredients.json", id.c_str());
				return false;
			}
		}
		lastError.clear();
		return true;
	}

	bool SimData::LoadFromDirectory(const std::string& folder)
	{
		struct FileJob
		{
			const char* name;
			bool        required;
			bool (SimData::*loader)(const std::string&);
		};
		const FileJob jobs[] = {
			{"Drinks.json", true, &SimData::LoadDrinks},   {"Ingredients.json", true, &SimData::LoadIngredients},
			{"Recipes.json", true, &SimData::LoadRecipes}, {"Scenarios.json", true, &SimData::LoadScenarios},
			{"Grading.json", true, &SimData::LoadGrading}, {"Simulation.json", false, &SimData::LoadConfig}};

		for (const FileJob& job : jobs)
		{
			const std::string path = folder + "/" + job.name;
			std::string       text;
			if (!ReadFile(path, text))
			{
				if (!job.required) continue; // optional file: keep the default settings
				lastError = "Could not open " + path;
				return false;
			}
			if (!(this->*job.loader)(text)) return false;
		}
		return Validate();
	}

	const DrinkDef* SimData::FindDrink(const std::string& id) const
	{
		for (const DrinkDef& drink : drinks)
			if (drink.id == id) return &drink;
		return nullptr;
	}

	const IngredientDef* SimData::FindIngredient(const std::string& id) const
	{
		for (const IngredientDef& item : ingredients)
			if (item.id == id) return &item;
		return nullptr;
	}

	const RecipeDef* SimData::FindRecipe(const std::string& id) const
	{
		const auto it = recipes.find(id);
		return it == recipes.end() ? nullptr : &it->second;
	}
}
