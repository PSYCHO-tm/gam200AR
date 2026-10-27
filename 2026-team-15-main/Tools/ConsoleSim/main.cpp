/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

Console demo for the coffee simulation (M1G06).
This is NOT part of the game. It is a small program that drives the simulation from the keyboard
or from a scripted "bot barista", so you can SEE the orders, stock, step grading and report.

    console_sim                         bot plays the "Normal" scenario (fast, same result every run)
    console_sim --scenario Rushhour     pick another scenario from Scenarios.json
    console_sim --guided                YOU are the barista: the program asks for every amount, in real time
    console_sim --guided --duration 90 --patience 30   change the shift length / customer patience (seconds)
    console_sim --interactive           YOU are the barista (type commands, "help" lists them)
    console_sim --speed 5               slow the bot down: 5 simulated seconds per real second
    console_sim --seed 99               different customers (0 = random every run)
    console_sim --assets <folder>       where the JSON files are

====================================================================================== */

#include "Simulation/simulation.hpp"
#include "guided.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace core;

namespace
{
	// ---------------------------------------------------------------------------------
	// Finding the JSON folder
	// ---------------------------------------------------------------------------------
	bool HasDrinksFile(const std::string& folder)
	{
		std::ifstream file(folder + "/Drinks.json");
		return static_cast<bool>(file);
	}

	std::string FindAssetsFolder(const std::string& requested)
	{
		std::vector<std::string> candidates;
		if (!requested.empty()) candidates.push_back(requested);
#ifdef KOPI_ASSETS_DIR
		candidates.push_back(KOPI_ASSETS_DIR);
#endif
		candidates.push_back("Assets/Json");
		candidates.push_back("../Assets/Json");
		candidates.push_back("../../Assets/Json");
		candidates.push_back("../../../Assets/Json");

		for (const std::string& folder : candidates)
			if (HasDrinksFile(folder)) return folder;
		return std::string();
	}

	// ---------------------------------------------------------------------------------
	// Time control: run the simulation forward in small 0.1 s steps
	// ---------------------------------------------------------------------------------
	void Advance(Simulation& sim, float seconds, double speed)
	{
		const int steps = static_cast<int>(seconds * 10.0f + 0.5f);
		for (int i = 0; i < steps && !sim.IsFinished(); ++i)
		{
			sim.Update(0.1f);
			if (speed > 0.0) std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(100.0 / speed)));
		}
	}

	// ---------------------------------------------------------------------------------
	// The bot barista: makes drinks following the recipe, with a few deliberate mistakes
	// so the grading has something to catch.
	// ---------------------------------------------------------------------------------
	enum class Mistake
	{
		None,                  // perfect drink
		WrongQuantity,         // pours 50% too much in the first step
		WrongSequenceThenSkip, // adds the 2nd ingredient too early, then forgets the last step
		Ignore                 // never makes this customer's drink (they will walk out)
	};

	Mistake MistakeFor(int orderId)
	{
		switch ((orderId - 1) % 4)
		{
			case 0:
				return Mistake::None;
			case 1:
				return Mistake::WrongQuantity;
			case 2:
				return Mistake::WrongSequenceThenSkip;
			default:
				return Mistake::Ignore;
		}
	}

	void SendAdd(Simulation& sim, const RecipeStep& step, float quantity)
	{
		// A good barista refills the shelf before it runs out
		for (int guard = 0; guard < 10 && sim.Stock().Level(step.ingredientId) < quantity; ++guard)
		{
			InteractionEvent refill;
			refill.object = step.ingredientId;
			refill.action = "refill";
			sim.OnInteraction(refill);
		}

		InteractionEvent add;
		add.object     = step.ingredientId;
		add.action     = "add";
		add.quantity   = quantity;
		add.attributes = step.attributes;
		sim.OnInteraction(add);
	}

	void MakeDrink(Simulation& sim, int orderId, Mistake mistake, double speed)
	{
		const Order* order = sim.Orders().FindOrder(orderId);
		if (order == nullptr) return;
		const DrinkDef  drink  = sim.Orders().GetDrink(*order); // copies, because the queue changes while we work
		const RecipeDef recipe = *sim.Data().FindRecipe(drink.recipeId);

		if (!sim.BeginOrder(orderId)) return;

		const std::size_t count     = recipe.steps.size();
		std::size_t       stepsToDo = count;

		if (mistake == Mistake::WrongSequenceThenSkip && count >= 2)
		{
			Advance(sim, 2.0f, speed);
			SendAdd(sim, recipe.steps[1], recipe.steps[1].amount); // too early!
			stepsToDo = count - 1;                                 // and the last step will be forgotten
		}

		for (std::size_t i = 0; i < stepsToDo; ++i)
		{
			Advance(sim, 2.0f, speed); // the barista takes 2 seconds per step
			float quantity = recipe.steps[i].amount;
			if (mistake == Mistake::WrongQuantity && i == 0) quantity *= 1.5f;
			SendAdd(sim, recipe.steps[i], quantity);
		}

		Advance(sim, 1.5f, speed);
		InteractionEvent serve;
		serve.object = drink.id;
		serve.action = "serve";
		sim.OnInteraction(serve);
	}

	void RunBot(Simulation& sim, double speed)
	{
		std::set<int> ignored;
		while (!sim.IsFinished())
		{
			int target = 0; // oldest customer we have not decided to ignore
			for (const Order& order : sim.Orders().GetQueue())
			{
				if (ignored.count(order.id) == 0)
				{
					target = order.id;
					break;
				}
			}

			if (target == 0)
			{
				Advance(sim, 0.5f, speed); // nobody to serve: wait for customers
				continue;
			}

			const Mistake mistake = MistakeFor(target);
			if (mistake == Mistake::Ignore)
			{
				ignored.insert(target);
				sim.Log(Fmt("(bot) is busy and ignores customer #%d on purpose", target));
				continue;
			}
			MakeDrink(sim, target, mistake, speed);
		}
	}

	// ---------------------------------------------------------------------------------
	// Interactive mode: you are the barista
	// ---------------------------------------------------------------------------------
	void PrintHelp()
	{
		std::cout << "\nCommands:\n"
					 "  status                         show the ticket rail, the shelf and what you are making\n"
					 "  begin <orderId>                pick up a ticket (if you skip this, the oldest ticket is used)\n"
					 "  recipe [orderId]               show the recipe steps\n"
					 "  add <ingredient> <qty> [Key=Value ...]\n"
					 "                                 e.g.  add coffee_beans 18 State=ground\n"
					 "  next                           do the next recipe step correctly (a shortcut for demos)\n"
					 "  serve [drinkId]                hand the drink over, e.g.  serve Kopi_O\n"
					 "  refill <ingredient>            restock the shelf\n"
					 "  wait <seconds>                 let time pass (customers arrive, patience runs out)\n"
					 "  report                         show the session report now\n"
					 "  quit                           end the shift and show the report\n\n";
	}

	void PrintStatus(const Simulation& sim)
	{
		std::cout << Fmt("\n--- STATUS at %.1fs ---\nTicket rail (%zu):\n", sim.Time(), sim.Orders().GetQueue().size());
		if (sim.Orders().GetQueue().empty()) std::cout << "  (empty)\n";
		for (const Order& order : sim.Orders().GetQueue())
			std::cout << Fmt(
				"  #%d %-10s patience %3.0f%%%s\n", order.id, sim.Orders().GetDrink(order).displayName.c_str(),
				order.PatienceRatio() * 100.0f,
				(sim.Prep().IsActive() && sim.Prep().GetOrderId() == order.id) ? "   <- you are making this" : "");
		std::cout << Fmt("Customers still to arrive: %d\n", sim.Orders().GetCustomersLeftToArrive());

		std::cout << "Shelf:\n";
		for (const auto& [id, level] : sim.Stock().GetLevels())
		{
			const IngredientDef* item = sim.Data().FindIngredient(id);
			std::cout << Fmt("  %-16s %7g %s%s\n", id.c_str(), level, item ? item->units.c_str() : "",
			                 sim.Stock().IsLow(id) ? "   LOW" : "");
		}

		if (sim.Prep().IsActive())
		{
			const RecipeStep* next = sim.Prep().GetExpectedStep();
			std::cout << Fmt("Making order #%d, next step: %s\n", sim.Prep().GetOrderId(),
			                 next ? sim.DescribeStep(*next).c_str() : "(all steps done - serve it!)");
		}
		std::cout << "\n";
	}

	void PrintRecipe(const Simulation& sim, int orderId)
	{
		const Order* order = orderId > 0 ? sim.Orders().FindOrder(orderId) : sim.Orders().GetFront();
		if (sim.Prep().IsActive() && orderId <= 0) order = sim.Orders().FindOrder(sim.Prep().GetOrderId());
		if (order == nullptr)
		{
			std::cout << "No such ticket.\n";
			return;
		}
		const DrinkDef&  drink  = sim.Orders().GetDrink(*order);
		const RecipeDef* recipe = sim.Data().FindRecipe(drink.recipeId);
		std::cout << Fmt("Recipe for %s (order #%d):\n", drink.displayName.c_str(), order->id);
		for (std::size_t i = 0; i < recipe->steps.size(); ++i)
			std::cout << Fmt("  %zu. %s\n", i + 1, sim.DescribeStep(recipe->steps[i]).c_str());
	}

	void RunInteractive(Simulation& sim)
	{
		PrintHelp();
		std::string line;
		while (!sim.IsFinished())
		{
			std::cout << "> " << std::flush;
			if (!std::getline(std::cin, line)) break;

			std::istringstream words(line);
			std::string        command;
			words >> command;
			if (command.empty()) continue;

			if (command == "quit" || command == "exit") break;
			else if (command == "help")
				PrintHelp();
			else if (command == "status")
				PrintStatus(sim);
			else if (command == "report")
				std::cout << sim.FormatReport();
			else if (command == "recipe")
			{
				int id = 0;
				words >> id;
				PrintRecipe(sim, id);
			}
			else if (command == "begin")
			{
				int id = 0;
				if (!(words >> id)) std::cout << "Usage: begin <orderId>\n";
				else
					sim.BeginOrder(id);
			}
			else if (command == "wait")
			{
				float seconds = 0.0f;
				if (!(words >> seconds) || seconds < 0.0f) std::cout << "Usage: wait <seconds>\n";
				else
					Advance(sim, seconds, 0.0);
			}
			else if (command == "refill")
			{
				InteractionEvent e;
				e.action = "refill";
				words >> e.object;
				sim.OnInteraction(e);
			}
			else if (command == "serve")
			{
				InteractionEvent e;
				e.action = "serve";
				words >> e.object; // optional
				sim.OnInteraction(e);
			}
			else if (command == "next")
			{
				if (!sim.Prep().IsActive() && sim.Orders().GetFront() != nullptr)
					sim.BeginOrder(sim.Orders().GetFront()->id);
				const RecipeStep* step = sim.Prep().GetExpectedStep();
				if (step == nullptr) std::cout << "Nothing left to add - type serve.\n";
				else
					SendAdd(sim, *step, step->amount);
			}
			else if (command == "add")
			{
				InteractionEvent e;
				e.action = "add";
				if (!(words >> e.object >> e.quantity))
				{
					std::cout << "Usage: add <ingredient> <qty> [Key=Value ...]\n";
					continue;
				}
				std::string pair;
				while (words >> pair)
				{
					const std::size_t eq = pair.find('=');
					if (eq != std::string::npos) e.attributes[pair.substr(0, eq)] = pair.substr(eq + 1);
				}
				sim.OnInteraction(e);
			}
			else
				std::cout << "Unknown command. Type help.\n";
		}
	}
}

int main(int argc, char** argv)
{
	std::string assetsArg;
	std::string scenarioName = "Normal";
	bool        interactive  = false;
	bool        guided       = false;
	float       duration     = 60.0f; // guided mode: shift length in seconds
	float       patience     = -1.0f; // guided mode: customer patience in seconds (negative = use Simulation.json)
	bool        seedGiven    = false;
	unsigned    seed         = 0;
	double      speed        = 0.0;

	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--interactive") interactive = true;
		else if (arg == "--guided")
			guided = true;
		else if (arg == "--duration" && i + 1 < argc)
			duration = static_cast<float>(std::atof(argv[++i]));
		else if (arg == "--patience" && i + 1 < argc)
			patience = static_cast<float>(std::atof(argv[++i]));
		else if (arg == "--scenario" && i + 1 < argc)
			scenarioName = argv[++i];
		else if (arg == "--assets" && i + 1 < argc)
			assetsArg = argv[++i];
		else if (arg == "--seed" && i + 1 < argc)
		{
			seed      = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
			seedGiven = true;
		}
		else if (arg == "--speed" && i + 1 < argc)
			speed = std::atof(argv[++i]);
		else
		{
			std::cout << "Usage: console_sim [--scenario Tutorial|Normal|Rushhour] [--guided] [--duration S] "
			             "[--patience S] [--interactive] [--speed X] [--seed N] "
						 "[--assets <folder>]\n";
			return arg == "--help" ? 0 : 1;
		}
	}

	// ---- 1. Load the data ----
	const std::string folder = FindAssetsFolder(assetsArg);
	if (folder.empty())
	{
		std::cout << "Could not find Drinks.json. Run from the repo root or pass --assets <folder>.\n";
		return 1;
	}

	SimData data;
	if (!data.LoadFromDirectory(folder))
	{
		std::cout << "DATA ERROR: " << data.GetLastError() << "\n";
		return 1;
	}
	if (seedGiven) data.config.seed = seed;

	// Guided mode: a fixed-length shift with endless customers, and a wrong pour has to be redone
	if (guided)
	{
		const auto found = data.scenarios.find(scenarioName);
		if (found != data.scenarios.end()) found->second.timeLimitSec = duration;
		data.config.repeatCustomers = true;
		if (patience > 0.0f) data.config.orderLifetime = patience;
	}

	// ---- 2. Start the simulation ----
	Simulation sim;
	sim.onLog = [](const std::string& text) { std::cout << text << "\n"; };
	sim.SetRetryOnMistake(guided);
	if (!sim.Init(data, scenarioName))
	{
		std::cout << sim.GetLastError() << "\n";
		return 1;
	}

	const ScenarioDef& scenario = data.scenarios.at(scenarioName);
	std::cout << "============================ KOPI-TWIN CONSOLE SIM ============================\n"
			  << Fmt("Loaded %zu drinks, %zu recipes, %zu ingredients, %zu scenarios from %s\n", data.drinks.size(),
	                 data.recipes.size(), data.ingredients.size(), data.scenarios.size(), folder.c_str())
			  << Fmt("Scenario %s: ", scenarioName.c_str());
	for (const ScenarioOrder& so : scenario.orders)
		std::cout << so.count << " x " << so.drinkId << "  ";
	std::cout << Fmt("| time limit %.0fs\n", scenario.timeLimitSec)
			  << Fmt("Tuning (Simulation.json): a customer every %.0fs, patience %.0fs, max %zu tickets, seed %u, "
	                 "%zu condition(s)\n",
	                 data.config.spawnInterval, data.config.orderLifetime, data.config.maxQueueSize, data.config.seed,
	                 data.config.conditions.size())
			  << "================================================================================\n";

	// ---- 3. Run it ----
	if (guided) RunGuided(sim);
	else if (interactive)
		RunInteractive(sim);
	else
		RunBot(sim, speed);

	std::cout << "\n" << sim.FormatReport();
	return 0;
}
