/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

====================================================================================== */

#include "Simulation/inventory.hpp"

namespace core
{
	void Inventory::Init(const std::vector<IngredientDef>&       ingredients,
	                     const std::map<std::string, StockRule>& newRules)
	{
		levels.clear();
		rules.clear();
		refillCount = 0;

		for (const IngredientDef& item : ingredients)
		{
			const auto      found = newRules.find(item.id);
			const StockRule rule  = (found != newRules.end()) ? found->second : StockRule{};
			rules[item.id]        = rule;
			levels[item.id]       = rule.start;
		}
	}

	float Inventory::Level(const std::string& id) const
	{
		const auto it = levels.find(id);
		return it == levels.end() ? 0.0f : it->second;
	}

	bool Inventory::IsLow(const std::string& id) const
	{
		const auto rule = rules.find(id);
		return rule != rules.end() && Level(id) < rule->second.lowThreshold;
	}

	bool Inventory::Consume(const std::string& id, float amount)
	{
		const auto it = levels.find(id);
		if (it == levels.end() || amount < 0.0f || it->second < amount)
			return false; // not enough on the shelf: nothing changes
		it->second -= amount;
		return true;
	}

	float Inventory::Refill(const std::string& id)
	{
		const auto it = levels.find(id);
		if (it == levels.end()) return 0.0f;
		const float added = rules[id].refillAmount;
		it->second += added;
		++refillCount;
		return added;
	}
}
