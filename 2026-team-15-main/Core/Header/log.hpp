#pragma once
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#ifndef KOPI_LOG_DIR
  #define KOPI_LOG_DIR "."                 // CMake sets this to <build>/logs
#endif

#if defined(__FILE_NAME__)
  #define LOG_FILE_NAME __FILE_NAME__
#else
  #define LOG_FILE_NAME __FILE__           // MSVC fallback
#endif

namespace Log
{
    inline std::ofstream& File() { static std::ofstream f; return f; }

    // Redirect the log (tests use this). Replaces the file's contents.
    inline bool SetFile(const std::string& path)
    {
        auto& f = File();
        if (f.is_open()) f.close();
        f.open(path, std::ios::out | std::ios::trunc);
        return f.is_open();
    }

    // ISO 8601 local time with milliseconds, e.g. 2026-10-25T13:08:21.042
    inline std::string Timestamp()
    {
        using namespace std::chrono;
        const auto now = system_clock::now();
        const std::time_t t = system_clock::to_time_t(now);
        const int ms = static_cast<int>(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
        std::tm tm{};
    #if defined(_WIN32)
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif
        char date[32], out[48];
        std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &tm);
        std::snprintf(out, sizeof(out), "%s.%03d", date, ms);
        return out;
    }

    // Console + file. The file opens on first use; every line is flushed.
    inline void Write(const std::string& line)
    {
        static std::mutex m;
        std::lock_guard<std::mutex> lock(m);

        std::cout << line << '\n';
        auto& f = File();
        if (!f.is_open())
        {
            std::error_code ec;
            std::filesystem::create_directories(KOPI_LOG_DIR, ec);
            f.open(std::string(KOPI_LOG_DIR) + "/kopitwin_log.txt", std::ios::out | std::ios::app);
        }
        if (f.is_open()) { f << (line + "\n"); f.flush(); }
    }
}

#define LOG(tag, ...) \
    do { std::ostringstream logStream_; \
         logStream_ << Log::Timestamp() << " " << tag << " " << LOG_FILE_NAME << ":" << __LINE__ << " " << __VA_ARGS__; \
         Log::Write(logStream_.str()); } while (0)

#define LOG_INFO(...)  LOG("INFO ", __VA_ARGS__)
#define LOG_DEBUG(...) LOG("DEBUG", __VA_ARGS__)
#define LOG_WARN(...)  LOG("WARN ", __VA_ARGS__)
#define LOG_ERROR(...) LOG("ERROR", __VA_ARGS__)