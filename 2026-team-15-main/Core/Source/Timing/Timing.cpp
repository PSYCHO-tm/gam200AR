#include "Timing/Timing.hpp"

#include "Log/Log.hpp"
#include <cstdio>

namespace Core
{
	void TimingRegistry::Record(const std::string& name, double ms)
	{
		Stat& stat = Stats[name];
		stat.Last  = ms;
		stat.Avg   = (stat.Count == 0) ? ms : (stat.Avg * 0.95 + ms * 0.05);
		++stat.Count;
	}

	double TimingRegistry::LastMs(const std::string& name) const
	{
		const auto found = Stats.find(name);
		return found == Stats.end() ? 0.0 : found->second.Last;
	}

	double TimingRegistry::AvgMs(const std::string& name) const
	{
		const auto found = Stats.find(name);
		return found == Stats.end() ? 0.0 : found->second.Avg;
	}

	std::vector<std::string> TimingRegistry::Names() const
	{
		std::vector<std::string> names;
		for (const auto& entry : Stats)
			names.push_back(entry.first);
		return names;
	}

	void TimingRegistry::LogAll() const
	{
		for (const auto& entry : Stats)
		{
			char line[160];
			std::snprintf(line, sizeof(line), "timing %-24s last %8.4f ms  avg %8.4f ms  count %llu",
			              entry.first.c_str(), entry.second.Last, entry.second.Avg,
			              static_cast<unsigned long long>(entry.second.Count));
			LogInfo(line);
		}
	}
}
