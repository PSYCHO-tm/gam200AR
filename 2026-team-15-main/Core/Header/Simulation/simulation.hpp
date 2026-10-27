/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

Simulation = the shop manager that ties everything together (M1G06 Domain Simulation & Assessment Core).

    Customers arrive      -> OrderSystem   (queue, patience)
    Shelf                 -> Inventory     (depletes and refills)
    Following the recipe  -> PrepTracker   (grades each step, times it)
    Rush hour etc.        -> conditions from Simulation.json
    End of shift          -> SessionReport (accuracy, time, waste, grade)

How other code uses it:
    Simulation sim;
    sim.Init(data, "Normal");
    // every frame
    sim.Update(dt);
    // whenever the player does something (M1G05 sends this)
    Verdict v = sim.OnInteraction({ "coffee_beans", "add", 18, {{"State", "ground"}} });

It never prints by itself. It sends text to `onLog`, so a console, a UI or Android Logcat can show it.

====================================================================================== */

#pragma once

#include "Simulation/inventory.hpp"
#include "Simulation/orderSystem.hpp"
#include "Simulation/prepTracker.hpp"
#include "Simulation/simData.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace core
{
	struct SessionReport
	{
		std::string scenario;
		std::string gradingId;
		double      durationSec{0.0};
		float       timeLimitSec{0.0f};
		bool        timeUp{false};

		int customersArrived{0};
		int ordersServed{0};
		int ordersExpired{0};
		int stillWaiting{0};

		int         stepsRequired{0};  // steps of EVERY customer that arrived
		int         stepsOfServed{0};  // steps of the drinks that were served
		int         stepsCorrect{0};   // correct steps on served drinks
		float       accuracyPct{0.0f}; // stepsCorrect / stepsOfServed
		float       scorePct{0.0f};    // stepsCorrect / stepsRequired (ignoring a customer costs you)
		std::string grade;

		float avgDrinkSeconds{0.0f};
		float avgStepSeconds{0.0f};

		std::map<std::string, int>   errorCounts; // by ErrorName
		std::map<std::string, float> waste;       // ingredient id -> amount
		int                          refills{0};

		std::vector<PrepResult> drinks;
	};

	class Simulation
	{
	public:
		using LogFn = std::function<void(const std::string&)>;
		LogFn onLog; // assign a function that shows text (std::cout, Logcat, an on-screen console...)

		Simulation()                  = default;
		Simulation(const Simulation&) = delete; // the callbacks inside point at "this", so never copy or move it
		Simulation& operator=(const Simulation&) = delete;

		// ---- Start ----------------------------------------------------------------------------------
		// Copies the data, so the SimData you pass in can go away afterwards.
		// Returns false and sets GetLastError() if the scenario name does not exist.
		bool Init(const SimData& data, const std::string& scenarioName);

		// ---- Every frame -----------------------------------------------------------------------------
		void Update(float dt);

		// ---- Whenever the player does something ----------------------------------------------------
		bool    BeginOrder(int orderId); // pick up a ticket (if you never call this, the oldest ticket is used)
		void    SetRetryOnMistake(bool enable) { retryOnMistake = enable; } // true: a wrong pour must be redone
		void    EndShift();                                                 // stop now (e.g. the player typed quit)
		Verdict OnInteraction(const InteractionEvent& event);               // "add", "serve" or "refill"

		// ---- Info --------------------------------------------------------------------------------------
		bool               IsFinished() const { return finished; }
		double             Time() const { return time; }
		float              GetTimeLimitSec() const { return scenario.timeLimitSec; }
		const OrderSystem& Orders() const { return orders; }
		const Inventory&   Stock() const { return inventory; }
		const PrepTracker& Prep() const { return prep; }
		const SimData&     Data() const { return data; }
		const std::string& GetLastError() const { return lastError; }

		std::string DescribeStep(const RecipeStep& step) const; // "Add 18 g Coffee Beans [State=ground]"
		void        Log(const std::string& text) const;         // prints with a time stamp

		SessionReport BuildReport() const;
		std::string   FormatReport() const; // the report as a text table

	private:
		void ApplyConditions();
		void CheckFinished();
		void Record(const PrepResult& result);
		void HandleExpired(const Order& order);

		SimData     data;
		ScenarioDef scenario;
		OrderSystem orders;
		Inventory   inventory;
		PrepTracker prep;

		double                  time{0.0};
		bool                    finished{false};
		bool                    retryOnMistake{false};
		bool                    timeUp{false};
		int                     requiredSteps{0};
		std::vector<PrepResult> results;
		std::vector<bool>       conditionActive;
		std::string             lastError;
	};
}
