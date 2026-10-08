// log.hpp - logging to console and a log file. No heap-allocated global state.
#pragma once
#include <string>

namespace Core
{
	enum class LOG_LEVEL
	{
		INFO,
		WARN,
		ERR
	};

	// Opens (truncates) the log file. Console logging works even if this returns false.
	bool LogInit(const std::string& filePath);
	void LogShutdown();
	void Log(LOG_LEVEL level, const std::string& message);

	inline void LogInfo(const std::string& message) { Log(LOG_LEVEL::INFO, message); }
	inline void LogWarn(const std::string& message) { Log(LOG_LEVEL::WARN, message); }
	inline void LogError(const std::string& message) { Log(LOG_LEVEL::ERR, message); }
}
