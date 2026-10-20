#pragma once

// M1G05 scene: the InteractionEngine + EquipmentRenderer + a fixed camera, behind four calls the platform shell makes.
// Shared by the Android shell and the desktop/host check, so both show the same thing.
//
// Thread safety: Android calls Touch from the UI thread and Frame from the GL thread, so both take the same lock.
// Frame/Resize/Init need the GL context current.
//
// Until M1G04 supplies a real anchor pose the camera is fixed and the anchor is identity ("fake anchor", M1G04 criterion).

#include <mutex>
#include <string>

#include "render/equipmentRenderer.hpp"
#include "sim/equipment/equipmentDef.hpp"
#include "sim/interaction/interactionEngine.hpp"

namespace Render
{
	class KopiScene
	{
	public:
		// readText: path as written in the data files (e.g. "Assets/Json/Equipment.json") -> file text.
		// Safe to call again after the GL context is recreated; it reloads everything.
		bool Init(Sim::TextReader const& readText, std::string& error);

		void Resize(int width, int height);

		// The shell owns the loop: call Update once per fixed tick (as often as it needs), Draw once per frame.
		void Update(float dt);
		void Draw();

		// Update(dt) then Draw(), for shells that do not run a fixed timestep (the Android one today).
		void Frame(float dt);

		// A button press for the selected equipment (an interaction id from Equipment.json, e.g. "pour_water").
		// Returns the engine's message. The resulting TimedEvent is queued for Engine().DrainEvents().
		std::string Press(std::string const& action);

		// Android MotionEvent action codes: 0 = DOWN. Only DOWN picks. Returns a short message ("" if ignored).
		std::string Touch(int action, float x, float y);

		// Same camera the picker uses, exposed so tests can aim a touch at a piece of equipment.
		Sim::Mat4 View() const;
		Sim::Mat4 Projection() const;

		Sim::InteractionEngine&       Engine() { return Interaction; }
		Sim::InteractionEngine const& Engine() const { return Interaction; }

	private:
		mutable std::mutex     Lock;
		Sim::InteractionEngine Interaction;
		EquipmentRenderer      Renderer;
		int                    Width  = 1;
		int                    Height = 1;
	};
}
