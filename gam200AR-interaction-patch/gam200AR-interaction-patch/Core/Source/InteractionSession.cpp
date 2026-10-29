#include "InteractionSession.hpp"

#include <algorithm>

namespace
{
    constexpr float errorSeconds = 3.0f; // how long a mistake stays on the equipment

    std::string AssessmentAction(InteractionDef const& interaction)
    {
        if (!interaction.assessAs.empty())
        {
            return interaction.assessAs;
        }
        return interaction.ingredient.empty() ? interaction.action : "add";
    }
}

core::InteractionEvent ToAssessmentEvent(InteractionDef const& interaction, float quantity)
{
    core::InteractionEvent event;
    event.action     = AssessmentAction(interaction);
    event.object     = interaction.ingredient; // empty for "serve": the assessment core then accepts any drink
    event.quantity   = quantity;
    event.attributes = interaction.attributes;
    return event;
}

Sim::OrderView BuildOrderView(core::Simulation const& simulation)
{
    Sim::OrderView view;
    core::PrepTracker const& prep   = simulation.Prep();
    core::OrderSystem const& orders = simulation.Orders();

    core::Order const* order = prep.IsActive() ? orders.FindOrder(prep.GetOrderId()) : orders.GetFront();
    if (order == nullptr)
    {
        return view; // "No active order"
    }

    core::DrinkDef const&  drink  = orders.GetDrink(*order);
    core::RecipeDef const* recipe = simulation.Data().FindRecipe(drink.recipeId);

    view.OrderName       = drink.displayName;
    view.StepCount       = recipe != nullptr ? static_cast<int>(recipe->steps.size()) : 0;
    view.StepSecondsLeft = std::max(0.0f, order->lifetime - order->timeAlive);

    std::size_t const next = prep.IsActive() ? prep.GetNextStepIndex() : 0;
    if (recipe != nullptr && next < recipe->steps.size())
    {
        view.StepIndex = static_cast<int>(next) + 1;
        view.StepName  = simulation.DescribeStep(recipe->steps[next]);
    }
    else
    {
        view.StepIndex = view.StepCount; // every ingredient is in: the last thing left is to serve
        view.StepName  = "Serve the drink";
    }
    return view;
}

bool InteractionSession::Init(EquipmentLibrary newLibrary, core::SimData const& data, std::string const& scenario,
                              std::string& error)
{
    std::lock_guard<std::mutex> guard(lock);
    if (!simulation.Init(data, scenario))
    {
        error = simulation.GetLastError();
        return false;
    }
    library = std::move(newLibrary);
    selected.clear();
    history.clear();
    errors.clear();
    lastMessage.clear();
    ready = true;
    return true;
}

void InteractionSession::Update(float dt)
{
    std::lock_guard<std::mutex> guard(lock);
    if (!ready)
    {
        return;
    }
    simulation.Update(dt);
    for (ActiveError& active : errors)
    {
        active.secondsLeft -= dt;
    }
    errors.erase(std::remove_if(errors.begin(), errors.end(),
                                [](ActiveError const& active) { return active.secondsLeft <= 0.0f; }),
                 errors.end());
}

void InteractionSession::Select(std::string const& equipmentId)
{
    std::lock_guard<std::mutex> guard(lock);
    selected = library.Find(equipmentId) != nullptr ? equipmentId : std::string{};
}

std::string InteractionSession::Selected() const
{
    std::lock_guard<std::mutex> guard(lock);
    return selected;
}

EquipmentDef const* InteractionSession::FindSelectedDef() const
{
    return selected.empty() ? nullptr : library.Find(selected);
}

std::vector<InteractionDef> InteractionSession::Buttons() const
{
    std::lock_guard<std::mutex> guard(lock);
    EquipmentDef const*         def = FindSelectedDef();
    return def != nullptr ? def->interactions : std::vector<InteractionDef>{};
}

std::string InteractionSession::Press(std::string const& action, std::optional<float> quantity)
{
    std::lock_guard<std::mutex> guard(lock);
    if (!ready)
    {
        return "the simulation is not loaded";
    }
    if (simulation.IsFinished())
    {
        return "the shift is over"; // nothing is graded any more, so nothing is recorded either
    }
    EquipmentDef const* def = FindSelectedDef();
    if (def == nullptr)
    {
        return "no equipment selected";
    }
    auto const found = std::find_if(def->interactions.begin(), def->interactions.end(),
                                    [&](InteractionDef const& interaction) { return interaction.action == action; });
    if (found == def->interactions.end())
    {
        return def->name + " cannot '" + action + "'";
    }

    // 1. The timed event: when, where, what, how much.
    float const            amount = quantity.value_or(found->quantity);
    core::InteractionEvent assessed = ToAssessmentEvent(*found, amount);
    history.push_back(TimedEvent{simulation.Time(), def->id, assessed.object, assessed.action, amount});

    // 2. The assessment core grades it now.
    core::Verdict const verdict = simulation.OnInteraction(assessed);
    lastMessage                 = verdict.message;

    // 3. A mistake is pinned to the equipment the player used. The renderer reads it on its next frame.
    if (verdict.hasError)
    {
        errors.push_back(ActiveError{def->id, core::ErrorName(verdict.errorType), verdict.message, errorSeconds});
    }
    return {};
}

Sim::OrderView InteractionSession::Order() const
{
    std::lock_guard<std::mutex> guard(lock);
    return ready ? BuildOrderView(simulation) : Sim::OrderView{};
}

std::vector<TimedEvent> InteractionSession::History() const
{
    std::lock_guard<std::mutex> guard(lock);
    return history;
}

std::vector<ActiveError> InteractionSession::Errors() const
{
    std::lock_guard<std::mutex> guard(lock);
    return errors;
}

bool InteractionSession::HasError(std::string const& equipmentId) const
{
    std::lock_guard<std::mutex> guard(lock);
    return std::any_of(errors.begin(), errors.end(),
                       [&](ActiveError const& active) { return active.equipment == equipmentId; });
}

std::string InteractionSession::LastMessage() const
{
    std::lock_guard<std::mutex> guard(lock);
    return lastMessage;
}

double InteractionSession::Time() const
{
    std::lock_guard<std::mutex> guard(lock);
    return simulation.Time();
}
