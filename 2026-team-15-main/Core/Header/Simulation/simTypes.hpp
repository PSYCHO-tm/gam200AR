/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

Plain data structs for the coffee simulation (M1G06).
Think of these as the "cards" in a kopi stall:
    DrinkDef    = a line on the menu
    RecipeDef   = the recipe card (ordered steps)
    SimConfig   = the manager's tuning sheet (how busy, how patient, how much stock)
No JSON and no printing in here. simData.cpp fills these from the JSON files.

====================================================================================== */

#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace core
{
	// printf-style helper that returns a std::string, e.g. Fmt("%d cups", 3)
	inline std::string Fmt(const char* fmt, ...)
	{
		char    buffer[512];
		va_list args;
		va_start(args, fmt);
		std::vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
		return std::string(buffer);
	}

	using AttributeMap = std::map<std::string, std::string>; // e.g. { "State" : "ground" }

	enum class DrinkStatus
	{
		Hot,
		Iced
	};

	// One line on the menu (from Drinks.json)
	struct DrinkDef
	{
		std::string id;          // "Kopi_O"          the JSON key
		std::string displayName; // "Kopi O"          nice text for the screen
		std::string recipeId;    // "Kopi_O_recipe"   which recipe card to use (Recipes.json)
		DrinkStatus status{DrinkStatus::Hot};
	};

	// One ingredient on the shelf (from Ingredients.json)
	struct IngredientDef
	{
		std::string id;          // "coffee_beans"
		std::string displayName; // "Coffee Beans"
		std::string units;       // "g"
	};

	// One step of a recipe = "add this much of this ingredient (in this state)"
	struct RecipeStep
	{
		std::string  ingredientId;
		float        amount{0.0f};
		AttributeMap attributes; // e.g. { "State" : "hot" }. The player's action must match every entry.
	};

	// The recipe card. Steps are in the same order as in Recipes.json, and the order matters!
	struct RecipeDef
	{
		std::string             id;
		std::vector<RecipeStep> steps;
	};

	// "Normal" scenario says: 2 x Kopi, 2 x Kopi_O ...
	struct ScenarioOrder
	{
		std::string drinkId;
		int         count{1};
	};

	struct ScenarioDef
	{
		std::string                name;
		int                        order{0};
		float                      timeLimitSec{0.0f}; // 0 = no time limit
		std::string                gradingId;
		std::vector<ScenarioOrder> orders;
	};

	// "A" needs 90 points and so on (Grading.json)
	struct GradeBand
	{
		std::string grade;
		float       minScore{0.0f};
	};

	// How one ingredient's shelf behaves (Simulation.json)
	struct StockRule
	{
		float start{100.0f};        // amount on the shelf at the start of the shift
		float refillAmount{100.0f}; // how much one refill adds
		float lowThreshold{20.0f};  // below this the shelf shouts "LOW STOCK"
	};

	// A temporary change in the workplace, e.g. "Rush Hour" (Simulation.json)
	struct Condition
	{
		std::string name;
		float       startSec{0.0f};
		float       durationSec{0.0f};
		float       orderRateMultiplier{1.0f}; // 2.0 = customers arrive twice as fast
		float       patienceMultiplier{1.0f};  // 0.5 = customers give up twice as fast
	};

	// The manager's tuning sheet. Everything here can be changed in Simulation.json without recompiling.
	struct SimConfig
	{
		float       initialDelay{1.0f};   // seconds before the first customer walks in
		float       spawnInterval{8.0f};  // seconds between customers
		float       orderLifetime{40.0f}; // seconds a customer waits before leaving
		std::size_t maxQueueSize{5};      // tickets on the rail at once
		unsigned    seed{0};              // 0 = random every run, any other number = same customers every run
		bool  repeatCustomers{false};   // true = when the scenario list runs out, start the list again (endless shift)
		float quantityTolerance{0.10f}; // 0.10 = a pour within +/-10% of the recipe still counts as correct

		std::map<std::string, StockRule> stock; // by ingredient id
		std::vector<Condition>           conditions;
	};

	// One thing the player did. M1G05 (interaction) sends these, M1G06 (this simulation) grades them.
	//   action "add"    object = ingredient id, quantity = amount, attributes = e.g. { State : ground }
	//   action "serve"  object = drink id (or empty)
	//   action "refill" object = ingredient id
	struct InteractionEvent
	{
		std::string  object;
		std::string  action;
		float        quantity{0.0f};
		AttributeMap attributes;
	};
}
