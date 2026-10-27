/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

Inventory = the shelf. Every ingredient has a level. Using it makes the level go down,
a refill makes it go up. The starting level, refill size and "low" warning come from data.

====================================================================================== */

#pragma once

#include "Simulation/simTypes.hpp"

#include <map>
#include <string>
#include <vector>

namespace core
{
	class Inventory
	{
	public:
		// Every ingredient gets a shelf. Ingredients missing from rules use the default StockRule.
		void Init(const std::vector<IngredientDef>& ingredients, const std::map<std::string, StockRule>& rules);

		bool  Known(const std::string& id) const { return levels.count(id) != 0; }
		float Level(const std::string& id) const;
		bool  IsLow(const std::string& id) const;

		// Takes `amount` off the shelf. Returns false (and changes nothing) if there is not enough.
		bool Consume(const std::string& id, float amount);

		// Adds the refill amount from the rules. Returns how much was added (0 if the ingredient is unknown).
		float Refill(const std::string& id);

		const std::map<std::string, float>& GetLevels() const { return levels; }
		int                                 GetRefillCount() const { return refillCount; }

	private:
		std::map<std::string, float>     levels;
		std::map<std::string, StockRule> rules;
		int                              refillCount{0};
	};
}
