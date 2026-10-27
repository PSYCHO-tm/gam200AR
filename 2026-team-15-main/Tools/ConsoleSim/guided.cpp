/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

====================================================================================== */

#include "guided.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace core;

namespace
{
	// ---------------------------------------------------------------------------------
	// Keyboard reader. It lives on its own thread so the shift clock keeps ticking
	// while you are thinking or typing (a normal std::getline would freeze everything).
	// ---------------------------------------------------------------------------------
	struct InputState
	{
		std::mutex              mutex;
		std::deque<std::string> lines;
		std::atomic<bool>       closed{false}; // true when the keyboard input has ended (Ctrl+D / pipe closed)
	};

	class InputReader
	{
	public:
		InputReader()
			: state(std::make_shared<InputState>())
		{
			std::shared_ptr<InputState> shared = state; // the thread keeps its own copy, so it is always safe
			std::thread(
				[shared]()
				{
					std::string line;
					while (std::getline(std::cin, line))
					{
						if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows line ending
						std::lock_guard<std::mutex> lock(shared->mutex);
						shared->lines.push_back(line);
					}
					shared->closed = true;
				})
				.detach();
		}

		bool TryPop(std::string& out)
		{
			std::lock_guard<std::mutex> lock(state->mutex);
			if (state->lines.empty()) return false;
			out = state->lines.front();
			state->lines.pop_front();
			return true;
		}

		bool IsClosed() const { return state->closed; }

	private:
		std::shared_ptr<InputState> state;
	};

	// ---------------------------------------------------------------------------------
	// Small text helpers
	// ---------------------------------------------------------------------------------
	std::string Trim(const std::string& text)
	{
		std::size_t first = 0;
		while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])))
			++first;
		std::size_t last = text.size();
		while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])))
			--last;
		return text.substr(first, last - first);
	}

	// lower case, letters and digits only: "Condensed Milk" and "condensed_milk" both become "condensedmilk"
	std::string Normalize(const std::string& text)
	{
		std::string out;
		for (char c : text)
			if (std::isalnum(static_cast<unsigned char>(c)))
				out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return out;
	}

	// How many single-letter edits turn a into b? ("condensemilk" -> "condensedmilk" = 1)
	int EditDistance(const std::string& a, const std::string& b)
	{
		std::vector<int> previous(b.size() + 1);
		std::vector<int> current(b.size() + 1);
		for (std::size_t j = 0; j <= b.size(); ++j)
			previous[j] = static_cast<int>(j);
		for (std::size_t i = 1; i <= a.size(); ++i)
		{
			current[0] = static_cast<int>(i);
			for (std::size_t j = 1; j <= b.size(); ++j)
			{
				const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
				current[j]     = std::min({previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost});
			}
			std::swap(previous, current);
		}
		return previous[b.size()];
	}

	bool ParseNumber(const std::string& text, float& out)
	{
		if (text.empty()) return false;
		char*       end   = nullptr;
		const float value = std::strtof(text.c_str(), &end);
		if (end == text.c_str() || *end != '\0') return false;
		out = value;
		return true;
	}

	// ---------------------------------------------------------------------------------
	// The guided shift
	// ---------------------------------------------------------------------------------
	class Guided
	{
	public:
		explicit Guided(Simulation& simulation)
			: sim(simulation)
		{
		}
		void Run();

	private:
		void Pump();                                 // move the simulation forward by the real time that has passed
		bool WaitLine(std::string& line);            // false = shift over, or the customer you were making left
		bool HandleCommand(const std::string& line); // "refill ..." or "quit". Returns true if it was a command.
		const IngredientDef* FindIngredient(const std::string& text) const;

		void PrintStock() const;
		void ServeCustomer(int orderId);
		bool DoStep(const RecipeStep& step);
		void LowStockPrompt();

		Simulation&                           sim;
		InputReader                           input;
		std::chrono::steady_clock::time_point lastTick{std::chrono::steady_clock::now()};
		int                                   activeOrderId{0};
	};

	void Guided::Pump()
	{
		const auto  now = std::chrono::steady_clock::now();
		const float dt  = std::chrono::duration<float>(now - lastTick).count();
		lastTick        = now;
		sim.Update(dt);
	}

	bool Guided::WaitLine(std::string& line)
	{
		for (;;)
		{
			Pump();
			if (sim.IsFinished()) return false;
			if (activeOrderId != 0 && !sim.Prep().IsActive()) return false; // the customer lost patience and left
			if (input.TryPop(line)) return true;
			if (input.IsClosed())
			{
				sim.EndShift(); // keyboard closed: nothing more can be typed
				return false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	}

	const IngredientDef* Guided::FindIngredient(const std::string& text) const
	{
		const std::string wanted = Normalize(text);
		if (wanted.empty()) return nullptr;

		const IngredientDef* best         = nullptr;
		int                  bestDistance = 3; // up to 2 typos are forgiven
		bool                 tie          = false;
		for (const IngredientDef& item : sim.Data().ingredients)
		{
			const int distance =
				std::min(EditDistance(wanted, Normalize(item.id)), EditDistance(wanted, Normalize(item.displayName)));
			if (distance == 0) return &item; // exact match wins
			if (distance < bestDistance)
			{
				best         = &item;
				bestDistance = distance;
				tie          = false;
			}
			else if (distance == bestDistance)
			{
				tie = true;
			}
		}
		return tie ? nullptr : best;
	}

	bool Guided::HandleCommand(const std::string& rawLine)
	{
		const std::string line  = Trim(rawLine);
		const std::string lower = [&]()
		{
			std::string copy = line;
			for (char& c : copy)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return copy;
		}();

		if (lower == "quit" || lower == "exit" || lower == "q")
		{
			sim.EndShift();
			return true;
		}

		if (lower.rfind("refill", 0) == 0)
		{
			const std::string name = Trim(line.substr(6));
			if (name.empty())
			{
				std::cout << "Say what to refill, e.g.  Refill Condensed Milk\n";
				return true;
			}
			const IngredientDef* item = FindIngredient(name);
			if (item == nullptr)
			{
				std::string options;
				for (const IngredientDef& i : sim.Data().ingredients)
					options += (options.empty() ? "" : ", ") + i.displayName;
				std::cout << "I do not know \"" << name << "\". Ingredients: " << options << "\n";
				return true;
			}

			InteractionEvent refill;
			refill.object = item->id;
			refill.action = "refill";
			sim.OnInteraction(refill);
			std::cout << Fmt("REFILLED %s (now %g %s)\n", item->displayName.c_str(), sim.Stock().Level(item->id),
			                 item->units.c_str());
			return true;
		}
		return false;
	}

	void Guided::PrintStock() const
	{
		std::cout << "Cafe Stock:\n";
		for (const IngredientDef& item : sim.Data().ingredients)
			std::cout << Fmt("  %-16s %g %s\n", item.displayName.c_str(), sim.Stock().Level(item.id),
			                 item.units.c_str());
	}

	// After a customer: if a shelf is low, warn and let the player refill (press ENTER to skip)
	void Guided::LowStockPrompt()
	{
		for (;;)
		{
			std::vector<const IngredientDef*> low;
			for (const IngredientDef& item : sim.Data().ingredients)
				if (sim.Stock().IsLow(item.id)) low.push_back(&item);
			if (low.empty() || sim.IsFinished()) return;

			std::cout << "\n";
			for (const IngredientDef* item : low)
				std::cout << Fmt("WARNING %s LOW (%g %s left)\n", item->displayName.c_str(),
				                 sim.Stock().Level(item->id), item->units.c_str());
			std::cout << "TYPE \"Refill <ingredient>\" to refill, or press ENTER to continue: " << std::flush;

			std::string line;
			if (!WaitLine(line)) return;
			if (Trim(line).empty()) return;
			if (!HandleCommand(line))
				std::cout << "Type e.g.  Refill " << low.front()->displayName << "  or press ENTER.\n";
		}
	}

	// One recipe step: keep asking until the pour is right (or the customer leaves)
	bool Guided::DoStep(const RecipeStep& step)
	{
		const IngredientDef* item  = sim.Data().FindIngredient(step.ingredientId);
		const std::string    name  = item ? item->displayName : step.ingredientId;
		const std::string    units = item ? item->units : "";

		for (;;)
		{
			if (sim.IsFinished() || !sim.Prep().IsActive()) return false;

			std::cout << Fmt("Add %g %s of %s: ", step.amount, units.c_str(), name.c_str()) << std::flush;
			std::string line;
			if (!WaitLine(line)) return false;
			line = Trim(line);
			if (line.empty()) continue;

			float quantity = 0.0f;
			if (ParseNumber(line, quantity))
			{
				if (quantity <= 0.0f)
				{
					std::cout << "Enter an amount above 0.\n";
					continue;
				}

				InteractionEvent add;
				add.object            = step.ingredientId;
				add.action            = "add";
				add.quantity          = quantity;
				add.attributes        = step.attributes; // the state (ground, hot...) is handled for you
				const Verdict verdict = sim.OnInteraction(add);

				if (verdict.stepAdvanced)
				{
					std::cout << Fmt("OK Step %zu/%zu done\n", sim.Prep().GetNextStepIndex(),
					                 sim.Prep().GetStepCount());
					return true;
				}
				if (verdict.hasError && verdict.errorType == ErrorType::WrongQuantity)
					std::cout << Fmt("ERROR, NEED %g %s BUT YOU USED %g %s, REDO\n", step.amount, units.c_str(),
					                 quantity, units.c_str());
				else if (verdict.hasError && verdict.errorType == ErrorType::OutOfStock)
					std::cout << Fmt("ERROR, NOT ENOUGH %s (only %g %s left). Type \"Refill %s\" first.\n",
					                 name.c_str(), sim.Stock().Level(step.ingredientId), units.c_str(), name.c_str());
				else
					std::cout << verdict.message << "\n";
				continue;
			}

			if (!HandleCommand(line)) std::cout << "Type an amount (a number), \"Refill <ingredient>\", or \"quit\".\n";
		}
	}

	void Guided::ServeCustomer(int orderId)
	{
		const Order* order = sim.Orders().FindOrder(orderId);
		if (order == nullptr) return;
		const DrinkDef drink  = sim.Orders().GetDrink(*order); // copies: the queue keeps changing while we work
		const float    waited = order->timeAlive;

		std::cout << Fmt("\nCustomer %d (patience %.0fs): %s", orderId, order->lifetime, drink.displayName.c_str());
		if (waited >= 1.0f) std::cout << Fmt("   (already waited %.0fs)", waited);
		std::cout << "\n";

		if (sim.Orders().GetQueue().size() > 1)
		{
			std::string others;
			for (const Order& other : sim.Orders().GetQueue())
				if (other.id != orderId)
					others += (others.empty() ? "" : ", ") +
					          Fmt("Customer %d (%s)", other.id, sim.Orders().GetDrink(other).displayName.c_str());
			std::cout << "Also waiting: " << others << "\n";
		}

		if (!sim.BeginOrder(orderId)) return;
		activeOrderId      = orderId;
		const double start = sim.Time();

		bool completed = true;
		while (const RecipeStep* step = sim.Prep().GetExpectedStep())
		{
			const RecipeStep copy = *step; // the tracker moves on after each step
			if (!DoStep(copy))
			{
				completed = false;
				break;
			}
		}

		if (completed && !sim.IsFinished())
		{
			InteractionEvent serve;
			serve.object          = drink.id;
			serve.action          = "serve";
			const Verdict verdict = sim.OnInteraction(serve);
			std::cout << Fmt("Customer %d completed in %.1f seconds.%s\n", orderId, sim.Time() - start,
			                 verdict.correct ? "  Perfect drink!" : "");
		}
		else if (!sim.IsFinished())
		{
			std::cout << Fmt("Customer %d left before you finished.\n", orderId);
		}
		activeOrderId = 0;
	}

	void Guided::Run()
	{
		// Only a few simulation messages are shown in guided mode: the rest is covered by the prompts below.
		sim.onLog = [](const std::string& text)
		{
			if (text.find("CONDITION") != std::string::npos || text.find("LEFT") != std::string::npos ||
			    text.find("still making") != std::string::npos || text.find("SHIFT OVER") != std::string::npos)
				std::cout << text << "\n";
		};

		PrintStock();
		std::cout << Fmt("\nThe shift is %.0f seconds long. ", sim.GetTimeLimitSec())
				  << "Type a number for each step. Type \"Refill <ingredient>\" to restock, or \"quit\" to stop.\n";
		std::cout << "Waiting for the first customer...\n";

		lastTick = std::chrono::steady_clock::now(); // the clock starts now
		while (!sim.IsFinished())
		{
			Pump();
			const Order* front = sim.Orders().GetFront();
			if (front == nullptr)
			{
				std::string line; // nobody to serve: only commands are accepted
				if (input.TryPop(line)) HandleCommand(line);
				if (input.IsClosed()) sim.EndShift();
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				continue;
			}

			ServeCustomer(front->id);
			if (!sim.IsFinished()) LowStockPrompt();
		}
		std::cout << "\n";
	}
}

void RunGuided(core::Simulation& sim)
{
	Guided guided(sim);
	guided.Run();
}
