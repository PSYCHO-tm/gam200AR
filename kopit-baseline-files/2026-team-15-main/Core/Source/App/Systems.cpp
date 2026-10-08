#include "App/Systems.hpp"

#include "Entity Component System/Components.hpp"

namespace Core
{
	void SpinSystem(Registry& registry, float dt, const InputState& input)
	{
		const float speedScale = 1.0f + input.SpinAxis;
		registry.ForEach<Transform, Spinner>(
			[&](Entity, Transform& transform, Spinner& spinner)
			{
				transform.PrevRotation = transform.Rotation;
				if (input.Reset)
				{
					transform.Rotation.y     = 0.0f;
					transform.PrevRotation.y = 0.0f;
					return;
				}
				transform.Rotation.y += spinner.DegreesPerSecond * speedScale * dt;
			});
	}
}
