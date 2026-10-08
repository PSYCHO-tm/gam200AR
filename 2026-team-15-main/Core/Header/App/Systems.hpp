// systems.hpp - systems work only on the components they need. No type checks, no wall clock.
#pragma once
#include "App/InputState.hpp"
#include "Entity Component System/Ecs.hpp"

namespace Core
{
	// Advances Spinner entities by the fixed timestep. Input changes the speed.
	void SpinSystem(Registry& registry, float dt, const InputState& input);
}
