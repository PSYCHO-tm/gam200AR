// input.hpp - the only input type the core knows. Platforms convert events into this.
#pragma once

namespace Core
{
	struct InputState
	{
		float SpinAxis = 0.0f;  // -1..1, changes spin speed of Spinner entities
		bool  Reset    = false; // resets spinner rotation to zero
	};
}
