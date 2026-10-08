#include "Log/Log.hpp"

#include <chrono>
#include <cstdio>
#include <mutex>

namespace Core
{
	namespace
	{
		struct LogState
		{
			std::FILE* File = nullptr;
			std::mutex Mutex;
		};

		LogState& GetState()
		{
			static LogState state;
			return state;
		}

		const char* LevelText(LOG_LEVEL level)
		{
			switch (level)
			{
				case LOG_LEVEL::INFO:
					return "INFO";
				case LOG_LEVEL::WARN:
					return "WARN";
				case LOG_LEVEL::ERR:
					return "ERROR";
			}
			return "?";
		}
	}

	bool LogInit(const std::string& filePath)
	{
		std::lock_guard<std::mutex> lock(GetState().Mutex);
		if (GetState().File != nullptr)
		{
			std::fclose(GetState().File);
			GetState().File = nullptr;
		}
#if defined(_MSC_VER)
		if (fopen_s(&GetState().File, filePath.c_str(), "w") != 0)
			GetState().File = nullptr;
#else
		GetState().File = std::fopen(filePath.c_str(), "w");
#endif
		return GetState().File != nullptr;
	}

	void LogShutdown()
	{
		std::lock_guard<std::mutex> lock(GetState().Mutex);
		if (GetState().File != nullptr)
		{
			std::fclose(GetState().File);
			GetState().File = nullptr;
		}
	}

	void Log(LOG_LEVEL level, const std::string& message)
	{
		using Clock                            = std::chrono::steady_clock;
		static const Clock::time_point start   = Clock::now();
		const double                   seconds = std::chrono::duration<double>(Clock::now() - start).count();

		std::lock_guard<std::mutex> lock(GetState().Mutex);
		std::FILE*                  console = (level == LOG_LEVEL::INFO) ? stdout : stderr;
		std::fprintf(console, "[%9.3f] [%-5s] %s\n", seconds, LevelText(level), message.c_str());
		if (GetState().File != nullptr)
		{
			std::fprintf(GetState().File, "[%9.3f] [%-5s] %s\n", seconds, LevelText(level), message.c_str());
			std::fflush(GetState().File);
		}
	}
}
