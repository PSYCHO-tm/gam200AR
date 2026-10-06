#include "../../Header/GameObject/InteractionEngine.hpp"

#include <algorithm>

namespace Core
{
	void InteractionEngine::SetEquipment(std::vector<EquipmentDef> defs)
	{
		Equipment     = std::move(defs);
		SelectedIndex = -1;
		RebuildWorldBounds();
	}

	void InteractionEngine::SetAnchorPose(Mat4 const& pose)
	{
		Anchor = pose;
		RebuildWorldBounds();
	}

	void InteractionEngine::RebuildWorldBounds()
	{
		WorldBounds.clear();
		for (EquipmentDef const& def : Equipment)
		{
			Aabb const local = {def.Bounds.Min * def.Scale + def.Position, def.Bounds.Max * def.Scale + def.Position};
			WorldBounds.push_back(TransformAabb(local, Anchor));
		}
	}

	Mat4 InteractionEngine::GetModelMatrix(std::size_t index) const
	{
		EquipmentDef const& def = Equipment[index];
		return Multiply(Anchor, Multiply(Translate(def.Position), ScaleUniform(def.Scale)));
	}

	void InteractionEngine::Update(float dt)
	{
		SimTime += dt;
		for (ErrorMarker& marker : Errors)
		{
			marker.TimeLeft -= dt;
		}
		Errors.erase(
			std::remove_if(Errors.begin(), Errors.end(), [](ErrorMarker const& m) { return m.TimeLeft <= 0.0f; }),
			Errors.end());
	}

	void InteractionEngine::HandleInput(InteractionInput const& input, Mat4 const& view, Mat4 const& projection,
	                                    float viewportWidth, float viewportHeight)
	{
		switch (input.Kind)
		{
			case InputKind::TOUCH_DOWN:
			{
				LastRay =
					ScreenPointToRay(input.ScreenX, input.ScreenY, viewportWidth, viewportHeight, view, projection);
				SelectedIndex = PickNearest(LastRay, WorldBounds).Index;
				break;
			}
			case InputKind::BUTTON_PRESS:
			{
				if (SelectedIndex < 0)
				{
					break;
				}
				EquipmentDef const& def = Equipment[static_cast<std::size_t>(SelectedIndex)];
				for (InteractionDef const& interaction : def.Interactions)
				{
					if (interaction.Button == input.Button)
					{
						Events.push_back({def.Id, interaction.Action, interaction.Quantity, SimTime});
						break;
					}
				}
				break;
			}
			case InputKind::TOUCH_UP:
			case InputKind::NONE:
				break;
		}
	}

	void InteractionEngine::ShowError(std::string const& objectId, std::string const& message, float seconds)
	{
		Errors.push_back({objectId, message, seconds});
	}

	std::vector<InteractionEvent> InteractionEngine::DrainEvents()
	{
		std::vector<InteractionEvent> drained;
		drained.swap(Events);
		return drained;
	}
}
