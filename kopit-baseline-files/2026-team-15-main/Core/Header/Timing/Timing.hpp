// timing.hpp - measures how long major systems take (M1G03).
#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Core
{
	class TimingRegistry
	{
	public:
		void                     Record(const std::string& name, double ms);
		double                   LastMs(const std::string& name) const;
		double                   AvgMs(const std::string& name) const;
		std::vector<std::string> Names() const;
		// Writes one log line per timer.
		void LogAll() const;

	private:
		struct Stat
		{
			double        Last  = 0.0;
			double        Avg   = 0.0;
			std::uint64_t Count = 0;
		};
		std::map<std::string, Stat> Stats;
	};

	class ScopedTimer
	{
	public:
		ScopedTimer(TimingRegistry& registry, std::string name)
			: Registry(registry),
			  TimerName(std::move(name)),
			  Start(std::chrono::steady_clock::now())
		{
		}
		~ScopedTimer()
		{
			const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - Start;
			Registry.Record(TimerName, elapsed.count());
		}
		ScopedTimer(const ScopedTimer&)            = delete;
		ScopedTimer& operator=(const ScopedTimer&) = delete;

	private:
		TimingRegistry&                       Registry;
		std::string                           TimerName;
		std::chrono::steady_clock::time_point Start;
	};
}
