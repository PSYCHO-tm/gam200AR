#pragma once

#include <string>

namespace Core
{
	// What the interaction engine hands to the assessment core (M1G06): who, what, how much, and when.
	// Time is simulation time (sum of the dt given to Update), never the wall clock.
	struct InteractionEvent
	{
		std::string Object;
		std::string Action;
		float       Quantity = 0.0f;
		double      Time     = 0.0;
	};

	// An error the assessment core found, shown at the object until TimeLeft runs out.
	struct ErrorMarker
	{
		std::string Object;
		std::string Message;
		float       TimeLeft = 0.0f;
	};

	// The current order and step, read by the native Android UI through the JNI bridge.
	struct OrderView
	{
		std::string OrderName;
		std::string StepName;
		int         StepIndex    = 0;
		int         StepCount    = 0;
		float       StepTimeLeft = 0.0f;
	};
}
