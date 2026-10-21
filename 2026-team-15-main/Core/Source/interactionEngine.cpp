#include "interactionEngine.hpp"

#include <algorithm>

namespace Sim
{
	void InteractionEngine::SetEquipment(EquipmentLibrary library)
	{
		Library = std::move(library);
		Selected.clear();
	}

	Mat4 InteractionEngine::GetModelMatrix(std::string const& id) const
	{
		EquipmentDef const* def = FindEquipment(Library, id);
		if (def == nullptr)
		{
			return Mat4{};
		}
		Mat4 const local = Multiply(Translate(def->Position),
		                            Multiply(RotateYDeg(def->YawDeg), Scale({def->Scale, def->Scale, def->Scale})));
		return Multiply(AnchorPose, local);
	}

	std::vector<PickCandidate> InteractionEngine::BuildCandidates() const
	{
		std::vector<PickCandidate> candidates;
		for (EquipmentDef const& def : Library.Items)
		{
			if (def.HasBounds)
			{
				candidates.push_back({def.Id, TransformAabb(def.LocalBounds, GetModelMatrix(def.Id))});
			}
		}
		return candidates;
	}

	InteractionResult InteractionEngine::Handle(InteractionInput const& input, Mat4 const& view, Mat4 const& projection)
	{
		InteractionResult result;
		if (input.Kind == INPUT_KIND::TOUCH)
		{
			Ray const ray = ScreenPointToRay(input.X, input.Y, input.ViewportW, input.ViewportH, view, projection);
			PickResult const hit = PickNearest(ray, BuildCandidates());
			Selected             = hit.Hit ? hit.Id : std::string{};
			result.Handled       = true;
			result.Selected      = Selected;
			result.Message       = hit.Hit ? "selected " + hit.Id : "nothing selected";
			return result;
		}

		result.Selected = Selected;
		if (Selected.empty())
		{
			result.Message = "no equipment selected";
			return result;
		}
		EquipmentDef const* def = FindEquipment(Library, Selected);
		if (def == nullptr)
		{
			result.Message = "selected equipment is not in the library";
			return result;
		}
		auto const found =
			std::find_if(def->Interactions.begin(), def->Interactions.end(),
		                 [&](InteractionDef const& interaction) { return interaction.Action == input.Action; });
		if (found == def->Interactions.end())
		{
			result.Message = def->Id + " cannot '" + input.Action + "'";
			return result;
		}

		TimedEvent const event{Time, def->Id, found->Action, found->Quantity};
		Outbox.push_back(event);
		History.push_back(event);
		result.Handled      = true;
		result.EventEmitted = true;
		result.Message      = def->Id + ": " + found->Action;
		return result;
	}

	void InteractionEngine::Update(float dt)
	{
		Time += static_cast<double>(dt);
		for (ActiveError& active : Errors)
		{
			active.SecondsLeft -= dt;
		}
		Errors.erase(std::remove_if(Errors.begin(), Errors.end(),
		                            [](ActiveError const& active) { return active.SecondsLeft <= 0.0f; }),
		             Errors.end());
	}

	void InteractionEngine::ReportError(AssessmentError const& error, float seconds)
	{
		Errors.push_back({error, seconds});
	}

	std::vector<TimedEvent> InteractionEngine::DrainEvents()
	{
		std::vector<TimedEvent> events = std::move(Outbox);
		Outbox.clear();
		return events;
	}
}
