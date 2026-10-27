/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

====================================================================================== */

#include "Simulation/simulation.hpp"

namespace core
{
	// ======================================================================================
	// START
	// ======================================================================================

	bool Simulation::Init(const SimData& newData, const std::string& scenarioName)
	{
		const auto found = newData.scenarios.find(scenarioName);
		if (found == newData.scenarios.end())
		{
			std::string names;
			for (const auto& [name, unused] : newData.scenarios)
			{
				(void)unused;
				names += (names.empty() ? "" : ", ") + name;
			}
			lastError = "Unknown scenario \"" + scenarioName + "\". Available: " + names;
			return false;
		}

		data     = newData;
		scenario = found->second;

		time          = 0.0;
		finished      = false;
		timeUp        = false;
		requiredSteps = 0;
		results.clear();
		conditionActive.assign(data.config.conditions.size(), false);
		prep = PrepTracker{};

		inventory.Init(data.ingredients, data.config.stock);
		orders.Init(data.drinks, &scenario, data.config);

		orders.onOrderSpawned = [this](const Order& order)
		{
			const DrinkDef&  drink  = orders.GetDrink(order);
			const RecipeDef* recipe = data.FindRecipe(drink.recipeId);
			requiredSteps += recipe ? static_cast<int>(recipe->steps.size()) : 0;
			Log(Fmt("CUSTOMER #%d arrives and wants %s (%s). Patience %.0fs.", order.id, drink.displayName.c_str(),
			        drink.status == DrinkStatus::Hot ? "Hot" : "Iced", order.lifetime));
		};
		orders.onOrderExpired = [this](const Order& order) { HandleExpired(order); };

		lastError.clear();
		return true;
	}

	// ======================================================================================
	// EVERY FRAME
	// ======================================================================================

	void Simulation::Update(float dt)
	{
		if (finished) return;

		time += dt; // simulation time only moves forward by the dt it is given (never reads the wall clock)
		ApplyConditions();
		orders.Update(dt);
		CheckFinished();
	}

	void Simulation::ApplyConditions()
	{
		float rate     = 1.0f;
		float patience = 1.0f;

		for (std::size_t i = 0; i < data.config.conditions.size(); ++i)
		{
			const Condition& c  = data.config.conditions[i];
			const bool       on = time >= c.startSec && time < c.startSec + c.durationSec;

			if (on != static_cast<bool>(conditionActive[i]))
			{
				conditionActive[i] = on;
				if (on)
					Log(Fmt("CONDITION: %s begins (customers arrive x%.1f, patience x%.1f)", c.name.c_str(),
					        c.orderRateMultiplier, c.patienceMultiplier));
				else
					Log(Fmt("CONDITION: %s ends", c.name.c_str()));
			}
			if (on)
			{
				rate *= c.orderRateMultiplier;
				patience *= c.patienceMultiplier;
			}
		}
		orders.SetRateMultiplier(rate);
		orders.SetPatienceMultiplier(patience);
	}

	void Simulation::CheckFinished()
	{
		if (finished) return;

		if (scenario.timeLimitSec > 0.0f && time >= scenario.timeLimitSec)
		{
			timeUp = true;
		}
		else if (!(orders.AllCustomersArrived() && orders.GetQueue().empty() && !prep.IsActive()))
		{
			return; // still customers to serve
		}

		if (prep.IsActive()) Record(prep.Abandon(time)); // time is up while a drink is half made

		finished = true;
		Log(timeUp ? "SHIFT OVER: time limit reached." : "SHIFT OVER: every customer is gone.");
	}

	void Simulation::HandleExpired(const Order& order)
	{
		Log(Fmt("CUSTOMER #%d (%s) ran out of patience and LEFT.", order.id,
		        orders.GetDrink(order).displayName.c_str()));
		if (prep.IsActive() && prep.GetOrderId() == order.id)
		{
			Log("  ...while you were still making their drink.");
			Record(prep.Abandon(time));
		}
	}

	void Simulation::EndShift()
	{
		if (finished) return;
		if (prep.IsActive()) Record(prep.Abandon(time));
		finished = true;
		Log("SHIFT OVER: ended early.");
	}

	void Simulation::Record(const PrepResult& result) { results.push_back(result); }

	// ======================================================================================
	// PLAYER ACTIONS
	// ======================================================================================

	bool Simulation::BeginOrder(int orderId)
	{
		if (finished) return false;
		if (prep.IsActive())
		{
			Log(Fmt("Finish order #%d first (serve it) before starting another.", prep.GetOrderId()));
			return false;
		}

		const Order* order = orders.FindOrder(orderId);
		if (order == nullptr)
		{
			Log(Fmt("There is no ticket #%d on the rail.", orderId));
			return false;
		}

		const DrinkDef&  drink  = orders.GetDrink(*order);
		const RecipeDef* recipe = data.FindRecipe(drink.recipeId);
		if (recipe == nullptr) return false; // cannot happen after SimData::Validate()

		prep.Begin(orderId, drink, *recipe, data.config.quantityTolerance, time, retryOnMistake);
		Log(Fmt("START order #%d: %s", orderId, drink.displayName.c_str()));
		for (std::size_t i = 0; i < recipe->steps.size(); ++i)
			Log(Fmt("    recipe step %zu: %s", i + 1, DescribeStep(recipe->steps[i]).c_str()));
		return true;
	}

	Verdict Simulation::OnInteraction(const InteractionEvent& event)
	{
		Verdict verdict;
		if (finished)
		{
			verdict.message = "The shift is over.";
			return verdict;
		}

		// ---- refill the shelf ----
		if (event.action == "refill")
		{
			if (!inventory.Known(event.object))
			{
				verdict.message = "ERROR: there is no ingredient called " + event.object;
				Log(verdict.message);
				return verdict;
			}
			const float          added = inventory.Refill(event.object);
			const IngredientDef* item  = data.FindIngredient(event.object);
			verdict.correct            = true;
			verdict.message =
				Fmt("REFILL %s +%g %s (now %g %s)", event.object.c_str(), added, item ? item->units.c_str() : "",
			        inventory.Level(event.object), item ? item->units.c_str() : "");
			Log(verdict.message);
			return verdict;
		}

		// ---- add an ingredient ----
		if (event.action == "add")
		{
			if (!prep.IsActive())
			{
				const Order* front = orders.GetFront();
				if (front == nullptr || !BeginOrder(front->id)) // nobody picked a ticket: take the oldest one
				{
					verdict.message = "There is no customer waiting.";
					Log(verdict.message);
					return verdict;
				}
			}

			if (inventory.Known(event.object))
			{
				const bool wasLow = inventory.IsLow(event.object);
				if (!inventory.Consume(event.object, event.quantity))
				{
					prep.RecordError(ErrorType::OutOfStock,
					                 Fmt("%s: needed %g but only %g on the shelf", event.object.c_str(), event.quantity,
					                     inventory.Level(event.object)),
					                 time);
					verdict.hasError  = true;
					verdict.errorType = ErrorType::OutOfStock;
					verdict.message   = Fmt("ERROR OutOfStock: only %g of %s left. Refill first!",
					                        inventory.Level(event.object), event.object.c_str());
					Log(verdict.message);
					return verdict;
				}
				if (!wasLow && inventory.IsLow(event.object))
				{
					const IngredientDef* item = data.FindIngredient(event.object);
					Log(Fmt("LOW STOCK: %s is down to %g %s", event.object.c_str(), inventory.Level(event.object),
					        item ? item->units.c_str() : ""));
				}
			}

			verdict = prep.Add(event, time);
			Log(verdict.message);
			return verdict;
		}

		// ---- hand the drink to the customer ----
		if (event.action == "serve")
		{
			if (!prep.IsActive())
			{
				verdict.message = "Nothing is being made. Start an order first.";
				Log(verdict.message);
				return verdict;
			}
			if (!event.object.empty() && event.object != prep.GetDrinkId())
			{
				prep.RecordError(
					ErrorType::WrongDrink,
					Fmt("served %s but the customer ordered %s", event.object.c_str(), prep.GetDrinkId().c_str()),
					time);
				verdict.hasError  = true;
				verdict.errorType = ErrorType::WrongDrink;
				verdict.message   = Fmt("ERROR WrongDrink: the customer ordered %s, not %s", prep.GetDrinkId().c_str(),
				                        event.object.c_str());
				Log(verdict.message);
				return verdict;
			}

			const int  orderId = prep.GetOrderId();
			PrepResult result  = prep.Finish(time);
			orders.ServeOrder(orderId);
			Record(result);

			verdict.correct      = result.errors.empty();
			verdict.stepAdvanced = true;
			verdict.message =
				Fmt("SERVED order #%d %s: %d/%d steps correct in %.1fs%s", result.orderId, result.drinkName.c_str(),
			        result.stepsCorrect, result.stepsTotal, result.seconds,
			        result.errors.empty() ? " - perfect!" : Fmt(" - %zu mistake(s)", result.errors.size()).c_str());
			Log(verdict.message);
			CheckFinished();
			return verdict;
		}

		verdict.message = "Unknown action \"" + event.action + "\" (use add, serve or refill).";
		Log(verdict.message);
		return verdict;
	}

	// ======================================================================================
	// TEXT HELPERS
	// ======================================================================================

	std::string Simulation::DescribeStep(const RecipeStep& step) const
	{
		const IngredientDef* item = data.FindIngredient(step.ingredientId);
		std::string          text = Fmt("Add %g %s %s", step.amount, item ? item->units.c_str() : "",
                               item ? item->displayName.c_str() : step.ingredientId.c_str());
		if (!step.attributes.empty())
		{
			text += " [";
			bool first = true;
			for (const auto& [name, value] : step.attributes)
			{
				text += (first ? "" : ", ") + name + "=" + value;
				first = false;
			}
			text += "]";
		}
		return text;
	}

	void Simulation::Log(const std::string& text) const
	{
		if (onLog) onLog(Fmt("[%6.1fs] ", time) + text);
	}

	// ======================================================================================
	// SESSION REPORT
	// ======================================================================================

	SessionReport Simulation::BuildReport() const
	{
		SessionReport r;
		r.scenario     = scenario.name;
		r.gradingId    = scenario.gradingId;
		r.durationSec  = time;
		r.timeLimitSec = scenario.timeLimitSec;
		r.timeUp       = timeUp;

		r.customersArrived = orders.GetSpawnedCount();
		r.ordersServed     = orders.GetServedCount();
		r.ordersExpired    = orders.GetExpiredCount();
		r.stillWaiting     = static_cast<int>(orders.GetQueue().size());
		r.stepsRequired    = requiredSteps;
		r.drinks           = results;
		r.refills          = inventory.GetRefillCount();

		float totalDrinkSeconds = 0.0f;
		float totalStepSeconds  = 0.0f;
		int   stepCount         = 0;
		for (const PrepResult& drink : results)
		{
			for (const GradedError& error : drink.errors)
				++r.errorCounts[ErrorName(error.type)];
			for (const auto& [ingredient, amount] : drink.waste)
				r.waste[ingredient] += amount;

			if (drink.served)
			{
				r.stepsOfServed += drink.stepsTotal;
				r.stepsCorrect += drink.stepsCorrect;
				totalDrinkSeconds += drink.seconds;
				for (float s : drink.stepSeconds)
				{
					totalStepSeconds += s;
					++stepCount;
				}
			}
		}

		r.accuracyPct     = r.stepsOfServed > 0 ? 100.0f * r.stepsCorrect / r.stepsOfServed : 0.0f;
		r.scorePct        = r.stepsRequired > 0 ? 100.0f * r.stepsCorrect / r.stepsRequired : 0.0f;
		r.avgDrinkSeconds = r.ordersServed > 0 ? totalDrinkSeconds / r.ordersServed : 0.0f;
		r.avgStepSeconds  = stepCount > 0 ? totalStepSeconds / stepCount : 0.0f;

		// Grade = the first band (best first) whose minimum score we reached
		const auto bands = data.grading.find(scenario.gradingId);
		if (bands != data.grading.end() && !bands->second.empty())
		{
			r.grade = bands->second.back().grade;
			for (const GradeBand& band : bands->second)
			{
				if (r.scorePct >= band.minScore)
				{
					r.grade = band.grade;
					break;
				}
			}
		}
		return r;
	}

	std::string Simulation::FormatReport() const
	{
		const SessionReport r = BuildReport();
		std::string         out;

		out += "================================ SESSION REPORT ================================\n";
		out += Fmt("Scenario       : %s%s\n", r.scenario.c_str(), r.timeUp ? "  (time limit reached)" : "");
		out += Fmt("Shift length   : %.1f s", r.durationSec);
		out += r.timeLimitSec > 0.0f ? Fmt("  (limit %.0f s)\n", r.timeLimitSec) : std::string("  (no limit)\n");
		out += Fmt("Customers      : %d arrived | %d served | %d left without a drink | %d still waiting\n",
		           r.customersArrived, r.ordersServed, r.ordersExpired, r.stillWaiting);
		out += Fmt("Accuracy       : %.1f %% of steps correct on served drinks (%d/%d)\n", r.accuracyPct,
		           r.stepsCorrect, r.stepsOfServed);
		out += Fmt("Overall score  : %.1f %% = correct steps / steps every customer needed (%d/%d)   GRADE: %s\n",
		           r.scorePct, r.stepsCorrect, r.stepsRequired, r.grade.c_str());
		out +=
			Fmt("Speed          : %.1f s per drink on average, %.1f s per step\n", r.avgDrinkSeconds, r.avgStepSeconds);

		std::string mistakes;
		for (const auto& [name, count] : r.errorCounts)
			mistakes += (mistakes.empty() ? "" : ", ") + name + " x" + std::to_string(count);
		out += "Mistakes       : " + (mistakes.empty() ? std::string("none") : mistakes) + "\n";

		std::string waste;
		for (const auto& [id, amount] : r.waste)
		{
			const IngredientDef* item = data.FindIngredient(id);
			waste += (waste.empty() ? "" : ", ") + Fmt("%s %g %s", id.c_str(), amount, item ? item->units.c_str() : "");
		}
		out += "Waste          : " + (waste.empty() ? std::string("none") : waste) + "\n";

		std::string shelf;
		for (const auto& [id, level] : inventory.GetLevels())
		{
			const IngredientDef* item = data.FindIngredient(id);
			shelf += (shelf.empty() ? "" : ", ") + Fmt("%s %g %s", id.c_str(), level, item ? item->units.c_str() : "");
		}
		out += Fmt("Shelf          : %d refill(s). Left at the end: %s\n", r.refills, shelf.c_str());

		out += "\nDrink log:\n";
		if (r.drinks.empty()) out += "  (no drinks were made)\n";
		for (const PrepResult& d : r.drinks)
		{
			std::string kinds;
			for (const GradedError& e : d.errors)
				kinds += (kinds.empty() ? "" : ", ") + std::string(ErrorName(e.type));
			out += Fmt("  #%d %-10s %-7s steps %d/%d  %5.1fs  %s\n", d.orderId, d.drinkName.c_str(),
			           d.served ? "SERVED" : "LEFT", d.stepsCorrect, d.stepsTotal, d.seconds,
			           kinds.empty() ? "no mistakes" : kinds.c_str());
		}
		out += "================================================================================\n";
		return out;
	}
}
