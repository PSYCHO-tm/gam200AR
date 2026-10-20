#pragma once

// M1G05 interaction engine: turns player input into a selected object and timed events for the assessment core,
// and keeps the errors the assessment core reports until the renderer has shown them.
// Pure simulation: no GL, no window, no Android types, no wall clock (time only advances through Update).

#include <string>
#include <vector>

#include "sim/collider/picking.hpp"
#include "sim/equipment/equipmentDef.hpp"
#include "sim/math/mat4.hpp"

namespace Sim
{
	// The only input type the engine knows. The Android shell and the desktop viewer both build one of these.
	enum class INPUT_KIND
	{
		TOUCH, // X, Y are pixels (0,0 top-left) inside a ViewportW x ViewportH view
		BUTTON // Action is the id of an interaction offered by the selected equipment
	};

	struct InteractionInput
	{
		INPUT_KIND  Kind      = INPUT_KIND::TOUCH;
		float       X         = 0.0f;
		float       Y         = 0.0f;
		int         ViewportW = 1;
		int         ViewportH = 1;
		std::string Action;
	};

	// What the assessment core (M1G06) receives for every accepted interaction.
	struct TimedEvent
	{
		double      Time = 0.0; // seconds of simulation time
		std::string Object;     // equipment id
		std::string Action;
		float       Quantity = 0.0f;
	};

	// What the assessment core sends back when it finds a mistake.
	struct AssessmentError
	{
		std::string Object;  // equipment id the mistake happened at
		std::string Kind;    // e.g. "wrong_ingredient", "wrong_quantity", "wrong_sequence", "skipped_step"
		std::string Message; // text for the label
	};

	struct ActiveError
	{
		AssessmentError Error;
		float           SecondsLeft = 0.0f;
	};

	struct InteractionResult
	{
		bool        Handled      = false; // false: input was rejected (see Message)
		bool        EventEmitted = false; // true when a TimedEvent was queued
		std::string Selected;             // selection after this input
		std::string Message;
	};

	class InteractionEngine
	{
	public:
		void SetEquipment(EquipmentLibrary library);
		// Anchor-local to world. Identity on desktop; ARCore supplies it on the phone.
		void SetAnchorPose(Mat4 const& pose) { AnchorPose = pose; }
		Mat4 const& GetAnchorPose() const { return AnchorPose; }

		// Anchor pose * position * yaw * scale. Identity if the id is unknown.
		Mat4                       GetModelMatrix(std::string const& id) const;
		std::vector<PickCandidate> BuildCandidates() const;

		InteractionResult Handle(InteractionInput const& input, Mat4 const& view, Mat4 const& projection);
		void              Update(float dt);

		// Shown by the renderer from the next frame on, for 'seconds' of simulation time.
		void ReportError(AssessmentError const& error, float seconds = 3.0f);

		// Events not yet handed to the assessment core. Draining clears them.
		std::vector<TimedEvent> DrainEvents();

		EquipmentLibrary const&         GetEquipment() const { return Library; }
		std::string const&              GetSelected() const { return Selected; }
		std::vector<TimedEvent> const&  GetHistory() const { return History; }
		std::vector<ActiveError> const& GetActiveErrors() const { return Errors; }
		double                          GetTime() const { return Time; }

	private:
		EquipmentLibrary         Library;
		Mat4                     AnchorPose;
		std::string              Selected;
		std::vector<TimedEvent>  Outbox;
		std::vector<TimedEvent>  History;
		std::vector<ActiveError> Errors;
		double                   Time = 0.0;
	};
}
