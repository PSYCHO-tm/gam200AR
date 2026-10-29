#pragma once

// M1G05 interaction session: the one place where "what the player did" meets "what the assessment core says".
//
//   touch  -> (EquipmentScene picks with Raycast + PickNearest) -> Select(id)
//   button -> Press(action)  -> TimedEvent (equipment, object, action, quantity, time)
//                            -> core::Simulation::OnInteraction (M1G06 grades it)
//                            -> Verdict with an error -> an error at that equipment, shown from the next frame
//   order and step for the Android screen -> Order()
//
// Pure C++: no GL, no Android, no JNI, no wall clock (time only advances through Update), so a host test can drive
// the whole flow. Every public function takes the lock, because the UI thread (buttons, reading text) and the
// GL thread (Update, Draw) both use it.

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "equipmentDef.hpp"
#include "orderView.hpp"
#include "Simulation/simData.hpp"
#include "Simulation/simulation.hpp"

// What the assessment core receives for every accepted interaction.
struct TimedEvent
{
    double      time = 0.0;  // simulation seconds (the assessment core's own clock)
    std::string equipment;   // the piece the player used, e.g. "Kettle"
    std::string object;      // what was acted on, e.g. "water" (empty for "serve")
    std::string action;      // what the assessment core is told: "add", "serve" or "refill"
    float       quantity = 0.0f;
};

// A mistake the assessment core found, kept until its time runs out so the renderer can show it at the equipment.
struct ActiveError
{
    std::string equipment;
    std::string kind;    // core::ErrorName, e.g. "WrongQuantity"
    std::string message; // text for a label
    float       secondsLeft = 0.0f;
};

// The core-defined version of a button press. Android turns its click into one of these (see JNI-interaction.cpp);
// no Android event type ever gets this far.
core::InteractionEvent ToAssessmentEvent(InteractionDef const& interaction, float quantity);

// "Order: Kopi | Step 2/3: Add 2 tablespoon(s) Condensed Milk [Sweetened=Yes] | 38 s", built from the simulation.
Sim::OrderView BuildOrderView(core::Simulation const& simulation);

class InteractionSession
{
public:
    // Copies both arguments. Returns false (and says why) if the scenario does not exist.
    bool Init(EquipmentLibrary library, core::SimData const& data, std::string const& scenario, std::string& error);

    // Once per frame with the frame time. Advances the assessment core and ages the errors.
    void Update(float dt);

    // Selection comes from the touch ray-cast. An empty or unknown id clears it.
    void Select(std::string const& equipmentId);
    std::string Selected() const;

    // The interactions the selected piece offers (the data decides, not the code).
    std::vector<InteractionDef> Buttons() const;

    // A button press. 'quantity' overrides the data's quantity (for a held pour). Returns "" when accepted,
    // otherwise why it was rejected. An accepted press is graded at once, so an error shows from the next frame.
    std::string Press(std::string const& action, std::optional<float> quantity = std::nullopt);

    Sim::OrderView           Order() const;
    std::vector<TimedEvent>  History() const;
    std::vector<ActiveError> Errors() const;
    bool                     HasError(std::string const& equipmentId) const;
    std::string              LastMessage() const; // the assessment core's last verdict, correct or not
    double                   Time() const;

    // For tests and for the report screen.
    core::Simulation&       Simulation() { return simulation; }
    core::Simulation const& Simulation() const { return simulation; }

private:
    EquipmentDef const* FindSelectedDef() const; // caller holds the lock

    mutable std::mutex       lock;
    EquipmentLibrary         library;
    core::Simulation         simulation;
    std::string              selected;
    std::vector<TimedEvent>  history;
    std::vector<ActiveError> errors;
    std::string              lastMessage;
    bool                     ready = false;
};
