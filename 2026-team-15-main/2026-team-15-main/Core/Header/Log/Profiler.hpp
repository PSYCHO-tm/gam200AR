#pragma once
#include "Types.hpp"
#include "Time/Time.hpp"

// CPU-side section profiler. Single-threaded (GL thread only).
// Records, per frame, how long each named section took, and keeps the last
// kHistory frames so stats (last/avg/min/max) can be computed on demand.
namespace Profiler
{
    constexpr UI32 kMaxSections = 32;
    constexpr UI32 kHistory     = 120;
    constexpr UI32 kInvalid     = 0xFFFFFFFFu;

    struct SectionStats
    {
        const char* name;
        F32 lastMs, avgMs, minMs, maxMs;
    };

    void Init();                       // registers section 0 = "Frame"
    void BeginFrame();                 // call first thing in Core::Frame
    void EndFrame();                   // call last thing in Core::Frame

    UI32  Register(const char* name);   // returns a section id (idempotent per name)
    void Add(UI32 id, UI64 ns);          // add time to a section for this frame

    UI32 GetSectionCount();
    SectionStats GetStats(UI32 id);

    // Writes one line per section into buf. Returns characters written.
    UI32 Format(char* buf, UI32 size);
}

#ifdef PROFILER_ENABLED
struct ScopedTimer
{
    UI32 id;
    UI64 start;

    explicit ScopedTimer(UI32 sectionId) : id(sectionId), start(Timer::Now()) {}
    ~ScopedTimer() { Profiler::Add(id, Timer::Now() - start); }
};

/** ## pastes two tokens into one identifier. The reason for two layers is a preprocessor rule: an argument that is used with ## (or #) is not macro-expanded first.
 * Routing through PROFILER_CAT first forces __LINE__ to expand to its number, and then PROFILER_CAT2 does the paste on the already-expanded result. */
#define PROFILER_CAT2(a, b) a##b
#define PROFILER_CAT(a, b)  PROFILER_CAT2(a, b)

/** The static local registers the name once; after that the cost is two clock reads. */
#define PROFILE_SCOPE(name)                                                        \
    static const UI32 PROFILER_CAT(profId_, __LINE__) = Profiler::Register(name);   \
    const ScopedTimer PROFILER_CAT(profTimer_, __LINE__)(PROFILER_CAT(profId_, __LINE__))  // '\' means macro continues on next line.
#else
#define PROFILE_SCOPE(name) ((void)0)
#endif