#pragma once

// What the Android screen shows about the current order and step (M1G05 -> native UI).
// The assessment core (M1G06) fills it in; the JNI bridge reads it. Pure data, no Android types.

#include <string>

namespace Sim
{
	struct OrderView
	{
		std::string OrderName;
		std::string StepName;
		int         StepIndex       = 0; // 1-based, 0 when there is no step
		int         StepCount       = 0;
		float       StepSecondsLeft = 0.0f;
	};

	// One line for the screen, e.g. "Order: Kopi | Step 2/4: Add water | 20 s".
	// Built in C++ so the wording is identical on every platform and can be tested without a phone.
	std::string FormatOrderText(OrderView const& order);
}
