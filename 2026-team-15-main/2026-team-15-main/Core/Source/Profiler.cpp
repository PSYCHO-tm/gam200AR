//
// Created by Zac Y on 18/10/2026.
//
#include "Profiler.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>

namespace Profiler
{
    namespace
    {
        const char* names[kMaxSections];
        UI32  count = 0;
        UI64  accumNs[kMaxSections];                 // this frame, summed over all entries
        F32  history[kMaxSections][kHistory];       // ms per frame, ring buffer
        UI32  head = 0;                              // next slot to write
        UI32  filled = 0;                            // valid frames, up to kHistory
        UI64  frameStart = 0;
    }

    void Init()
    {
        count = 0; head = 0; filled = 0;
        std::memset(accumNs, 0, sizeof(accumNs));
        std::memset(history, 0, sizeof(history));
        Register("Frame");   // id 0: CPU work for the whole Core::Frame call
    }

    UI32 Register(const char* name)
    {
        for (UI32 i = 0; i < count; ++i)
            if (std::strcmp(names[i], name) == 0) return i;
        if (count >= kMaxSections) return kInvalid;
        names[count] = name;
        return count++;
    }

    void Add(UI32 id, UI64 ns) { if (id < count) accumNs[id] += ns; }

    void BeginFrame() { frameStart = Timer::Now(); }

    void EndFrame()
    {
        accumNs[0] = Timer::Now() - frameStart;
        for (UI32 i = 0; i < count; ++i)
        {
            history[i][head] = static_cast<F32>(accumNs[i]) * 1e-6f;   // ns -> ms
            accumNs[i] = 0;
        }
        head = (head + 1) % kHistory;
        if (filled < kHistory) ++filled;
    }

    UI32 GetSectionCount() { return count; }

    SectionStats GetStats(UI32 id)
    {
        SectionStats s{ "?", 0, 0, 0, 0 };
        if (id >= count || filled == 0) return s;

        s.name   = names[id];
        s.lastMs = history[id][(head + kHistory - 1) % kHistory];
        s.minMs  = history[id][0];
        s.maxMs  = history[id][0];
        F32 sum = 0.f;
        for (UI32 f = 0; f < filled; ++f)
        {
            const F32 v = history[id][f];
            sum += v;
            if (v < s.minMs) s.minMs = v;
            if (v > s.maxMs) s.maxMs = v;
        }
        s.avgMs = sum / static_cast<F32>(filled);
        return s;
    }

    UI32 Format(char* buf, UI32 size)
    {
        UI32 used{};
        for (UI32 i = 0; i < count && used + 1 < size; ++i)
        {
            const SectionStats s = GetStats(i);
            const int n = std::snprintf(buf + used, size - used,
                                        "%-12s %5.2f avg %5.2f max\n", s.name, s.avgMs, s.maxMs);
            if (n < 0 || static_cast<UI32>(n) >= size - used) break;
            used += static_cast<UI32>(n);
        }
        return used;
    }
}