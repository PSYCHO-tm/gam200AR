/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

PrepTracker = the examiner standing next to the barista with the recipe card.
It follows ONE drink being made. Every time the player does something (an InteractionEvent)
it checks that against the next step of the recipe, times each step, and remembers every mistake.

    Right ingredient, right amount, right state   -> step done, move to the next step
    Right ingredient, wrong amount or state      -> WrongQuantity / WrongState (step is done but flawed)
    An ingredient that is needed LATER           -> WrongSequence (step is NOT done)
    An ingredient that is not needed at all      -> WrongIngredient (step is NOT done)
    Serving before the last step                 -> SkippedStep (one error per missing step)

This is the ONLY place where steps are graded (M1G05 only sends events and shows feedback).

====================================================================================== */

#pragma once

#include "Simulation/simTypes.hpp"

#include <map>
#include <string>
#include <vector>

namespace core
{
	enum class ErrorType
	{
		WrongIngredient,
		WrongQuantity,
		WrongState,
		WrongSequence,
		SkippedStep,
		OutOfStock,
		WrongDrink,
		CustomerLeft
	};

	const char* ErrorName(ErrorType type);

	struct GradedError
	{
		ErrorType   type{ErrorType::WrongIngredient};
		std::string detail;
		double      time{0.0}; // simulation time when it happened
	};

	// The result of one drink (served or abandoned)
	struct PrepResult
	{
		int                          orderId{0};
		std::string                  drinkId;
		std::string                  drinkName;
		int                          stepsTotal{0};
		int                          stepsCorrect{0};
		float                        seconds{0.0f}; // from "started" to "served"
		std::vector<float>           stepSeconds;   // how long each finished step took
		std::map<std::string, float> waste;         // ingredient id -> amount wasted
		std::vector<GradedError>     errors;
		bool                         served{false}; // false = the customer left before it was served
	};

	// What the player should see right away after an action (M1G05 shows this at the object involved)
	struct Verdict
	{
		bool        correct{false};      // true = no mistake
		bool        stepAdvanced{false}; // true = the recipe moved on to the next step
		bool        hasError{false};     // true = a mistake was found (see errorType)
		ErrorType   errorType{ErrorType::WrongIngredient};
		std::string message;
	};

	class PrepTracker
	{
	public:
		void Begin(int orderId, const DrinkDef& drink, const RecipeDef& recipe, float quantityTolerance, double nowSec,
		           bool retryOnMistake = false);

		bool               IsActive() const { return active; }
		int                GetOrderId() const { return orderId; }
		const std::string& GetDrinkId() const { return drink.id; }
		std::size_t        GetNextStepIndex() const { return nextStep; }
		std::size_t        GetStepCount() const { return recipe.steps.size(); }
		const RecipeStep*  GetExpectedStep() const
		{
			return nextStep < recipe.steps.size() ? &recipe.steps[nextStep] : nullptr;
		}

		// Grade an "add ingredient" action
		Verdict Add(const InteractionEvent& event, double nowSec);

		// Record a mistake found by someone else (e.g. the shelf was empty)
		void RecordError(ErrorType type, const std::string& detail, double nowSec);

		PrepResult Finish(double nowSec);  // the drink was served: any steps not done count as skipped
		PrepResult Abandon(double nowSec); // the customer left before it was served

	private:
		bool        active{false};
		int         orderId{0};
		DrinkDef    drink;
		RecipeDef   recipe;
		float       tolerance{0.1f};
		double      startTime{0.0};
		double      lastStepTime{0.0};
		std::size_t nextStep{0};
		bool        retryOnMistake{false}; // true = a wrong pour does not advance the step, the player must redo it
		bool        stepHadError{false};   // a mistake happened on the step that is pending now
		PrepResult  result;
	};
}
