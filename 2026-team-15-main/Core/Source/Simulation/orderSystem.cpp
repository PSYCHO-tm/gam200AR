/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

====================================================================================== */

#include "Simulation/orderSystem.hpp"

#include <utility>

namespace core
{
	// ======================================================================================
	// Step 1: START A SHIFT
	// ======================================================================================

	void OrderSystem::Init(const std::vector<DrinkDef>& newMenu, const ScenarioDef* scenario,
	                       const SimConfig& newConfig)
	{
		menu   = newMenu;
		config = newConfig;

		if (config.seed != 0) rng.seed(config.seed); // same seed = same customers every run
		else
			rng.seed(std::random_device{}()); // different customers every run

		// Today's customers come from the scenario: "2 x Kopi, 2 x Kopi_O" becomes [Kopi, Kopi, Kopi_O, Kopi_O]
		scenarioOrders.clear();
		if (scenario != nullptr) scenarioOrders = scenario->orders;
		endless = (scenario == nullptr);
		BuildPool();

		queue.clear();
		timeToNextSpawn    = config.initialDelay;
		rateMultiplier     = 1.0f;
		patienceMultiplier = 1.0f;
		nextOrderId        = 1;
		spawnedCount       = 0;
		servedCount        = 0;
		expiredCount       = 0;
	}

	void OrderSystem::BuildPool()
	{
		pool.clear();
		poolNext = 0;
		for (const ScenarioOrder& entry : scenarioOrders)
		{
			for (std::size_t i = 0; i < menu.size(); ++i)
			{
				if (menu[i].id == entry.drinkId)
				{
					for (int n = 0; n < entry.count; ++n)
						pool.push_back(i);
					break;
				}
			}
		}

		// Shuffle so customers do not arrive in the same order as the list (Fisher-Yates).
		// We use rng() directly (not std::shuffle) so the same seed gives the same result on every compiler.
		for (std::size_t i = pool.size(); i > 1; --i)
		{
			const std::size_t j = static_cast<std::size_t>(rng()) % i;
			std::swap(pool[i - 1], pool[j]);
		}
	}

	// ======================================================================================
	// Step 2: EVERY FRAME
	// ======================================================================================

	void OrderSystem::Update(float dt)
	{
		if (menu.empty()) return; // nothing on the menu, so no customers

		UpdateOrders(dt);     // customers already waiting get more impatient
		UpdateSpawnTimer(dt); // maybe a new customer walks in
	}

	void OrderSystem::UpdateOrders(float dt)
	{
		for (auto it = queue.begin(); it != queue.end();)
		{
			it->timeAlive += dt;

			if (it->timeAlive >= it->lifetime)
			{
				++expiredCount;
				const Order expired = *it;             // copy, because erase() destroys the original
				it                  = queue.erase(it); // erase() gives back the next valid position
				if (onOrderExpired) onOrderExpired(expired);
			}
			else
			{
				++it;
			}
		}
	}

	void OrderSystem::UpdateSpawnTimer(float dt)
	{
		if (AllCustomersArrived()) return; // today's customers are all inside already

		timeToNextSpawn -= dt * rateMultiplier; // Rush Hour makes this count down faster
		if (timeToNextSpawn > 0.0f) return;

		if (queue.size() >= config.maxQueueSize)
		{
			timeToNextSpawn = 0.0f; // rail is full: the customer waits outside and comes in when a spot frees up
			return;
		}

		SpawnOrder();
		timeToNextSpawn = config.spawnInterval; // restart the countdown
	}

	void OrderSystem::SpawnOrder()
	{
		if (!endless && config.repeatCustomers && poolNext >= pool.size()) BuildPool(); // new round of the same list

		Order order;
		order.id         = nextOrderId++;
		order.drinkIndex = endless ? static_cast<std::size_t>(rng()) % menu.size() : pool[poolNext++];
		order.timeAlive  = 0.0f;
		order.lifetime   = config.orderLifetime * patienceMultiplier;

		queue.push_back(order); // new customers join the back of the line
		++spawnedCount;

		if (onOrderSpawned) onOrderSpawned(order);
	}

	// ======================================================================================
	// SERVING
	// ======================================================================================

	bool OrderSystem::ServeOrder(int orderId)
	{
		for (auto it = queue.begin(); it != queue.end(); ++it)
		{
			if (it->id == orderId)
			{
				++servedCount;
				const Order served = *it;
				queue.erase(it);
				if (onOrderServed) onOrderServed(served);
				return true;
			}
		}
		return false; // no such ticket (maybe the customer already left)
	}

	const Order* OrderSystem::FindOrder(int orderId) const
	{
		for (const Order& order : queue)
			if (order.id == orderId) return &order;
		return nullptr;
	}
}
