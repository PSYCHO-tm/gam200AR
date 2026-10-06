#pragma once

#include <string>

namespace Core
{
	// The only input type the interaction engine understands. The platform layer (Android JNI, SDL, tests)
	// converts its own events into this; core code never sees an Android MotionEvent or an SDL_Event.
	enum class InputKind
	{
		NONE,
		TOUCH_DOWN, // ScreenX / ScreenY in pixels, origin top-left
		TOUCH_UP,
		BUTTON_PRESS // Button is the id of a native UI button, e.g. "pour"
	};

	struct InteractionInput
	{
		InputKind   Kind    = InputKind::NONE;
		float       ScreenX = 0.0f;
		float       ScreenY = 0.0f;
		std::string Button;
	};
}
