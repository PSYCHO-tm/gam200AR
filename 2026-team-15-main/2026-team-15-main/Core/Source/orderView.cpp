#include "orderView.hpp"

#include <cstdio>

namespace Sim
{
	std::string FormatOrderText(OrderView const& order)
	{
		if (order.OrderName.empty())
		{
			return "No active order";
		}
		char line[256];
		std::snprintf(line, sizeof(line), "Order: %s | Step %d/%d: %s | %.0f s", order.OrderName.c_str(),
		              order.StepIndex, order.StepCount, order.StepName.c_str(),
		              static_cast<double>(order.StepSecondsLeft));
		return line;
	}
}
