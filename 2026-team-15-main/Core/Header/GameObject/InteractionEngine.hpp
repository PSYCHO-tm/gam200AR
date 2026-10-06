#pragma once

#include <string>
#include <vector>

#include "../Button/InteractionInput.hpp"
#include "../Collider/Picking.hpp"
#include "EquipmentDef.hpp"
#include "InteractionEvent.hpp"

namespace Core
{
	class InteractionEngine
	{
	public:
		// Equipment lives at its local Position/Scale relative to the anchor. Bounds must already be filled in.
		void SetEquipment(std::vector<EquipmentDef> defs);
		void SetAnchorPose(Mat4 const& pose);

		// Advances simulation time and expires old error markers. Never reads the wall clock.
		void Update(float dt);

		// Touch selects the nearest equipment under the finger. A button press performs the selected equipment's
		// mapped interaction and queues a timed event.
		void HandleInput(InteractionInput const& input, Mat4 const& view, Mat4 const& projection, float viewportWidth,
		                 float viewportHeight);

		// Called by the assessment core. The marker is visible to the renderer on the very next frame.
		void ShowError(std::string const& objectId, std::string const& message, float seconds);

		void SetOrderView(OrderView const& order) { Order = order; }

		std::vector<InteractionEvent> DrainEvents();

		OrderView const&                 GetOrderView() const { return Order; }
		std::vector<EquipmentDef> const& GetEquipment() const { return Equipment; }
		std::vector<Aabb> const&         GetWorldBounds() const { return WorldBounds; }
		std::vector<ErrorMarker> const&  GetErrors() const { return Errors; }
		Ray const&                       GetLastRay() const { return LastRay; }
		int                              GetSelected() const { return SelectedIndex; }
		double                           GetTime() const { return SimTime; }
		Mat4 const&                      GetAnchorPose() const { return Anchor; }

		// anchor * translate(position) * scale, for the renderer
		Mat4 GetModelMatrix(std::size_t index) const;

	private:
		void RebuildWorldBounds();

		std::vector<EquipmentDef>     Equipment;
		std::vector<Aabb>             WorldBounds;
		std::vector<ErrorMarker>      Errors;
		std::vector<InteractionEvent> Events;
		OrderView                     Order;
		Mat4                          Anchor;
		Ray                           LastRay;
		int                           SelectedIndex = -1;
		double                        SimTime       = 0.0;
	};
}
