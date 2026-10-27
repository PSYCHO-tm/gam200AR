/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

Order system for the coffee shop (GAM200).
Think of it as the "ticket rail" in a kopi stall:
    - The scenario (Scenarios.json) says WHICH customers will come: "2 x Kopi, 2 x Kopi O"
    - OrderSystem is the DOOR: one customer walks in every few seconds, in shuffled order
    - The queue is the TICKET RAIL: tickets wait in order, and a customer who waits too long leaves

This class has no JSON and no printing, so it works the same in a console, a test, or on Android.

====================================================================================== */

#pragma once

#include "Simulation/simTypes.hpp"

#include <cstddef>
#include <deque>
#include <functional>
#include <random>
#include <vector>

namespace core
{
	// One ticket on the rail = one waiting customer
	struct Order
	{
		int         id{0};           // unique number for this ticket (1, 2, 3...)
		std::size_t drinkIndex{0};   // which drink on the menu (index into the menu given to Init)
		float       timeAlive{0.0f}; // seconds this customer has been waiting
		float       lifetime{0.0f};  // seconds the customer is willing to wait before leaving

		// 1.0 = just arrived (happy), 0.0 = about to leave (angry). Handy for a patience bar.
		float PatienceRatio() const
		{
			if (lifetime <= 0.0f) return 0.0f;
			const float ratio = 1.0f - (timeAlive / lifetime);
			return ratio < 0.0f ? 0.0f : ratio;
		}
	};

	class OrderSystem
	{
	public:
		using OrderCallback = std::function<void(const Order&)>;

		// ---- Step 1: start a shift ----------------------------------------------------------------
		// menu     = the drinks customers can ask for (a copy is kept)
		// scenario = who is coming (nullptr = endless random customers)
		// config   = spawn interval, patience, queue size, seed
		void Init(const std::vector<DrinkDef>& menu, const ScenarioDef* scenario, const SimConfig& config);

		// ---- Step 2: call every frame ---------------------------------------------------------------
		void Update(float dt); // dt = seconds since last frame

		// ---- Workplace conditions (e.g. Rush Hour) change these while the shift runs ------------------
		void SetRateMultiplier(float multiplier) { rateMultiplier = multiplier; } // 2.0 = customers arrive 2x faster
		void SetPatienceMultiplier(float multiplier)
		{
			patienceMultiplier = multiplier;
		} // applies to customers who arrive from now on

		// ---- Serving --------------------------------------------------------------------------------
		bool ServeOrder(int orderId); // take a ticket off the rail (counts as served)

		// ---- Read-only info for UI / other systems --------------------------------------------------
		const std::deque<Order>&     GetQueue() const { return queue; } // front = oldest customer
		const Order*                 GetFront() const { return queue.empty() ? nullptr : &queue.front(); }
		const Order*                 FindOrder(int orderId) const;
		const DrinkDef&              GetDrink(const Order& order) const { return menu[order.drinkIndex]; }
		const std::vector<DrinkDef>& GetMenu() const { return menu; }

		bool AllCustomersArrived() const
		{
			return !endless && !config.repeatCustomers && poolNext >= pool.size();
		} // nobody left to come in
		int GetCustomersLeftToArrive() const
		{
			return (endless || config.repeatCustomers) ? -1 : static_cast<int>(pool.size() - poolNext);
		}
		int GetSpawnedCount() const { return spawnedCount; }
		int GetServedCount() const { return servedCount; }
		int GetExpiredCount() const { return expiredCount; }

		// ---- Optional hooks: assign a lambda to react to events (print, play a sound, change score...) ----
		OrderCallback onOrderSpawned;
		OrderCallback onOrderServed;
		OrderCallback onOrderExpired;

	private:
		void UpdateOrders(float dt);
		void UpdateSpawnTimer(float dt);
		void SpawnOrder();
		void BuildPool(); // today's customers from the scenario list, shuffled

		std::vector<DrinkDef> menu;  // the menu
		std::deque<Order>     queue; // the ticket rail

		SimConfig                  config{};
		std::mt19937               rng{std::random_device{}()};
		std::vector<std::size_t>   pool;           // today's customers (menu indexes), shuffled
		std::vector<ScenarioOrder> scenarioOrders; // the scenario list, kept so the pool can be rebuilt
		std::size_t                poolNext{0};
		bool                       endless{false};

		float timeToNextSpawn{0.0f};
		float rateMultiplier{1.0f};
		float patienceMultiplier{1.0f};
		int   nextOrderId{1};
		int   spawnedCount{0};
		int   servedCount{0};
		int   expiredCount{0};
	};
}
