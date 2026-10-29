#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Engine.hpp"
#include "Profiler.hpp"
#include "Time.hpp"
#include "log.hpp"

namespace
{
    constexpr UI64 kFrameNs = 16'666'667;   // ~60 fps, in nanoseconds

    // Runs n frames on the fake clock. Returns the next unused timestamp.
    UI64 RunFrames(int n, UI64 start = 0)
    {
        UI64 t = start;
        for (int i = 0; i < n; ++i) { Core::Update(t); t += kFrameNs; }
        return t;
    }

    bool FindSection(const char* name, Profiler::SectionStats& out)
    {
        for (UI32 i = 0; i < Profiler::GetSectionCount(); ++i)
        {
            const Profiler::SectionStats s = Profiler::GetStats(i);
            if (std::strcmp(s.Name, name) == 0) { out = s; return true; }
        }
        return false;
    }
}

TEST_CASE("Engine starts, runs frames and shuts down")
{
    REQUIRE(Core::TryInitialize());
    RunFrames(10);

    CHECK(Timer::GetFrameCount() == 9);   // the first tick only seeds the clock
    CHECK_THAT(Timer::GetFrameTime(),
               Catch::Matchers::WithinAbs(0.016666667, 1e-6));
    Core::Exit();
}

TEST_CASE("Engine can restart cleanly")
{
    REQUIRE(Core::TryInitialize());
    Core::Exit();
    REQUIRE(Core::TryInitialize());       // second start after a shutdown
    RunFrames(3);
    Core::Exit();
}

TEST_CASE("Profiler measures the Update section")
{
    REQUIRE(Core::TryInitialize());
    RunFrames(10);

    Profiler::SectionStats update{};
    REQUIRE(FindSection("Update", update));   // fails if PROFILER_ENABLED is missing
    CHECK(update.AvgMs >= 0.f);
    CHECK(update.MaxMs >= update.AvgMs);

    Profiler::SectionStats frame{};
    REQUIRE(FindSection("Frame", frame));
    CHECK(frame.MaxMs > 0.f);
    Core::Exit();
}

TEST_CASE("Exit writes the timing report to the log")
{
    const auto logPath = std::filesystem::path(KOPI_LOG_DIR) / "kopitwin_log.txt";
    std::error_code ec;
    const auto before = std::filesystem::exists(logPath, ec)
                            ? std::filesystem::file_size(logPath, ec) : 0;

    REQUIRE(Core::TryInitialize());
    RunFrames(10);
    Core::Exit();

    std::ifstream in(logPath);
    in.seekg(static_cast<std::streamoff>(before));   // only read what this test added
    std::stringstream added;
    added << in.rdbuf();
    CHECK(added.str().find("PROFILE Update") != std::string::npos);
}

TEST_CASE("PauseTime stops game time")
{
    REQUIRE(Core::TryInitialize());
    UI64 t = RunFrames(3);

    Core::PauseTime(true);
    Core::Update(t);
    CHECK(Core::dt == 0.f);

    Core::PauseTime(false);
    Core::Update(t + kFrameNs);
    CHECK(Core::dt > 0.f);
    Core::Exit();
}