/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

====================================================================================== */

#include "Simulation/prepTracker.hpp"

#include <cmath>

namespace core
{
	const char* ErrorName(ErrorType type)
	{
		switch (type)
		{
			case ErrorType::WrongIngredient:
				return "WrongIngredient";
			case ErrorType::WrongQuantity:
				return "WrongQuantity";
			case ErrorType::WrongState:
				return "WrongState";
			case ErrorType::WrongSequence:
				return "WrongSequence";
			case ErrorType::SkippedStep:
				return "SkippedStep";
			case ErrorType::OutOfStock:
				return "OutOfStock";
			case ErrorType::WrongDrink:
				return "WrongDrink";
			case ErrorType::CustomerLeft:
				return "CustomerLeft";
		}
		return "Unknown";
	}

	void PrepTracker::Begin(int newOrderId, const DrinkDef& newDrink, const RecipeDef& newRecipe,
	                        float quantityTolerance, double nowSec, bool retryOnMistake_)
	{
		active         = true;
		orderId        = newOrderId;
		drink          = newDrink;
		recipe         = newRecipe;
		tolerance      = quantityTolerance;
		startTime      = nowSec;
		lastStepTime   = nowSec;
		nextStep       = 0;
		retryOnMistake = retryOnMistake_;
		stepHadError   = false;

		result            = PrepResult{};
		result.orderId    = newOrderId;
		result.drinkId    = newDrink.id;
		result.drinkName  = newDrink.displayName;
		result.stepsTotal = static_cast<int>(newRecipe.steps.size());
	}

	void PrepTracker::RecordError(ErrorType type, const std::string& detail, double nowSec)
	{
		GradedError error;
		error.type   = type;
		error.detail = detail;
		error.time   = nowSec;
		result.errors.push_back(std::move(error));
		stepHadError = true; // the step that is pending now is no longer "perfect"
	}

	Verdict PrepTracker::Add(const InteractionEvent& event, double nowSec)
	{
		Verdict verdict;
		if (!active)
		{
			verdict.message = "No drink is being made.";
			return verdict;
		}

		// The recipe is already finished, so anything else is an extra.
		if (nextStep >= recipe.steps.size())
		{
			RecordError(ErrorType::WrongIngredient, Fmt("recipe already complete, extra %s", event.object.c_str()),
			            nowSec);
			result.waste[event.object] += event.quantity;
			verdict.hasError  = true;
			verdict.errorType = ErrorType::WrongIngredient;
			verdict.message =
				Fmt("ERROR WrongIngredient: the drink is already complete, no more %s!", event.object.c_str());
			return verdict;
		}

		const RecipeStep& expected = recipe.steps[nextStep];

		// ---- The player added the ingredient the recipe asks for right now ----
		if (event.object == expected.ingredientId)
		{
			const bool quantityWrong = std::fabs(event.quantity - expected.amount) > tolerance * expected.amount;

			std::string stateProblem; // e.g. "State should be ground but was whole"
			for (const auto& [name, wanted] : expected.attributes)
			{
				const auto given = event.attributes.find(name);
				if (given == event.attributes.end() || given->second != wanted)
				{
					stateProblem = Fmt("%s should be %s but was %s", name.c_str(), wanted.c_str(),
					                   given == event.attributes.end() ? "missing" : given->second.c_str());
					break;
				}
			}

			float wasted = 0.0f;
			if (quantityWrong)
			{
				RecordError(ErrorType::WrongQuantity,
				            Fmt("%s: expected %g but got %g", event.object.c_str(), expected.amount, event.quantity),
				            nowSec);
				if (event.quantity > expected.amount)
					wasted = event.quantity - expected.amount; // over-pouring wastes the extra
			}
			if (!stateProblem.empty())
			{
				RecordError(ErrorType::WrongState, Fmt("%s: %s", event.object.c_str(), stateProblem.c_str()), nowSec);
				wasted = event.quantity; // wrong state: the whole scoop is ruined
			}
			if (wasted > 0.0f) result.waste[event.object] += wasted;

			const bool actionOk = !quantityWrong && stateProblem.empty();
			if (!actionOk)
			{
				verdict.hasError  = true;
				verdict.errorType = quantityWrong ? ErrorType::WrongQuantity : ErrorType::WrongState;
				if (retryOnMistake)
				{
					// Strict mode: this pour is wasted and the player must do the SAME step again
					if (quantityWrong)
						verdict.message = Fmt("ERROR WrongQuantity: %s needs %g but you used %g, try again",
						                      event.object.c_str(), expected.amount, event.quantity);
					else
						verdict.message =
							Fmt("ERROR WrongState: %s %s, try again", event.object.c_str(), stateProblem.c_str());
					return verdict;
				}
			}

			const float stepSeconds = static_cast<float>(nowSec - lastStepTime);
			result.stepSeconds.push_back(stepSeconds);
			lastStepTime = nowSec;
			++nextStep;

			// A step only counts as "correct" if nothing went wrong while it was pending (also not on earlier tries)
			if (actionOk && !stepHadError) ++result.stepsCorrect;
			stepHadError = false;

			verdict.stepAdvanced = true;
			verdict.correct      = actionOk;

			if (verdict.correct)
				verdict.message = Fmt("OK step %zu/%zu done: %s (%.1fs)", nextStep, recipe.steps.size(),
				                      event.object.c_str(), stepSeconds);
			else if (quantityWrong)
				verdict.message = Fmt("ERROR WrongQuantity: %s needs %g but you used %g", event.object.c_str(),
				                      expected.amount, event.quantity);
			else
				verdict.message = Fmt("ERROR WrongState: %s %s", event.object.c_str(), stateProblem.c_str());
			return verdict;
		}

		// ---- The player added something else ----
		bool neededLater = false;
		for (std::size_t i = nextStep + 1; i < recipe.steps.size(); ++i)
			if (recipe.steps[i].ingredientId == event.object) neededLater = true;

		const ErrorType type = neededLater ? ErrorType::WrongSequence : ErrorType::WrongIngredient;
		RecordError(type, Fmt("expected %s but got %s", expected.ingredientId.c_str(), event.object.c_str()), nowSec);
		result.waste[event.object] += event.quantity;
		verdict.hasError  = true;
		verdict.errorType = type;

		if (neededLater)
			verdict.message = Fmt("ERROR WrongSequence: %s comes later, the next step is %s", event.object.c_str(),
			                      expected.ingredientId.c_str());
		else
			verdict.message = Fmt("ERROR WrongIngredient: %s is not part of this recipe (next step is %s)",
			                      event.object.c_str(), expected.ingredientId.c_str());
		return verdict;
	}

	PrepResult PrepTracker::Finish(double nowSec)
	{
		for (std::size_t i = nextStep; i < recipe.steps.size(); ++i)
			RecordError(ErrorType::SkippedStep,
			            Fmt("step %zu skipped: %s", i + 1, recipe.steps[i].ingredientId.c_str()), nowSec);

		result.seconds = static_cast<float>(nowSec - startTime);
		result.served  = true;
		active         = false;
		return result;
	}

	PrepResult PrepTracker::Abandon(double nowSec)
	{
		RecordError(ErrorType::CustomerLeft, Fmt("customer left after %zu of %zu steps", nextStep, recipe.steps.size()),
		            nowSec);
		result.seconds = static_cast<float>(nowSec - startTime);
		result.served  = false;
		active         = false;
		return result;
	}
}
