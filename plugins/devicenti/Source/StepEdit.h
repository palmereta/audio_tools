#pragma once
// DeviceNTI V: edits of a pattern's steps and step locks (the panel, the SEQ page and live recording). No JUCE. A step
// keeps its locks contiguous in the pattern's lock pool, so every change rebuilds the pool in (track, lane, step)
// order; the pool holds at most kPoolLocks.

#include "dsp/Seq.h"
#include <algorithm>
#include <tuple>
#include <vector>

namespace dh::dv::stepedit {

struct LockRef { int track, lane, step, id; float v; };

inline std::vector<LockRef> allLocks (const Pattern& p)
{
    std::vector<LockRef> all;
    for (int t = 0; t < kTracks; ++t) for (int l = 0; l < kLanes; ++l) for (int i = 0; i < kMaxSteps; ++i)
    {
        const Step& s = p.t[(size_t) t].lane[(size_t) l][(size_t) i];
        for (int k = 0; k < s.lockCount; ++k)
        {
            const auto& L = p.pool[(size_t) std::min ((int) s.lockStart + k, kPoolLocks - 1)];
            all.push_back ({ t, l, i, L.id, L.v });
        }
    }
    return all;
}

inline void rebuildPool (Pattern& p, std::vector<LockRef> all)
{
    std::stable_sort (all.begin(), all.end(), [] (const LockRef& a, const LockRef& b) { return std::make_tuple (a.track, a.lane, a.step) < std::make_tuple (b.track, b.lane, b.step); });
    for (auto& tr : p.t) for (auto& ln : tr.lane) for (auto& s : ln) { s.lockCount = 0; s.lockStart = 0; }
    p.nLocks = 0;
    for (const auto& a : all)
    {
        if (p.nLocks >= kPoolLocks) break;
        Step& s = p.t[(size_t) a.track].lane[(size_t) a.lane][(size_t) a.step];
        if (s.lockCount == 255) continue;
        if (s.lockCount == 0) s.lockStart = p.nLocks;
        p.pool[(size_t) p.nLocks++] = { (int16_t) a.id, a.v };
        ++s.lockCount;
    }
}

// Set (or replace) one lock of a step.
inline void setLock (Pattern& p, int track, int lane, int step, int id, float value)
{
    auto all = allLocks (p);
    bool found = false;
    for (auto& a : all) if (a.track == track && a.lane == lane && a.step == step && a.id == id) { a.v = value; found = true; }
    if (! found) all.push_back ({ track, lane, step, id, value });
    rebuildPool (p, std::move (all));
}

// Remove the locks of a step (id < 0: all of them).
inline void clearLocks (Pattern& p, int track, int lane, int step, int id = -1)
{
    auto all = allLocks (p);
    all.erase (std::remove_if (all.begin(), all.end(), [&] (const LockRef& a) { return a.track == track && a.lane == lane && a.step == step && (id < 0 || a.id == id); }), all.end());
    rebuildPool (p, std::move (all));
}

inline bool hasLock (const Pattern& p, int track, int lane, int step, int id)
{
    const Step& s = p.t[(size_t) track].lane[(size_t) lane][(size_t) step];
    for (int k = 0; k < s.lockCount; ++k) if (p.pool[(size_t) std::min ((int) s.lockStart + k, kPoolLocks - 1)].id == id) return true;
    return false;
}

inline float lockValue (const Pattern& p, int track, int lane, int step, int id, float fallback)
{
    const Step& s = p.t[(size_t) track].lane[(size_t) lane][(size_t) step];
    for (int k = 0; k < s.lockCount; ++k) { const auto& L = p.pool[(size_t) std::min ((int) s.lockStart + k, kPoolLocks - 1)]; if (L.id == id) return L.v; }
    return fallback;
}

// Toggle a step: a new trig plays the given note (−1: the track's default note); removing it clears its locks.
inline void toggle (Pattern& p, int track, int lane, int step, int note = -1, int vel = 100)
{
    Step& s = p.t[(size_t) track].lane[(size_t) lane][(size_t) step];
    if (s.on()) { clearLocks (p, track, lane, step); p.t[(size_t) track].lane[(size_t) lane][(size_t) step] = Step(); return; }
    s = Step(); s.trig = 1; s.vel = (uint8_t) std::clamp (vel, 1, 127);
    if (note >= 0) { s.nNotes = 1; s.notes[0] = (int8_t) std::clamp (note, 0, 127); }
}

// A page of 16 steps (copy / paste / clear on the panel).
struct PageClip { bool valid = false; std::array<Step, 16> steps {}; std::vector<LockRef> locks; };
inline PageClip copyPage (const Pattern& p, int track, int lane, int page)
{
    PageClip c; c.valid = true;
    for (int i = 0; i < 16; ++i) { c.steps[(size_t) i] = p.t[(size_t) track].lane[(size_t) lane][(size_t) (page * 16 + i)]; c.steps[(size_t) i].lockCount = 0; c.steps[(size_t) i].lockStart = 0; }
    for (const auto& a : allLocks (p)) if (a.track == track && a.lane == lane && a.step / 16 == page) c.locks.push_back ({ 0, 0, a.step - page * 16, a.id, a.v });
    return c;
}
inline void clearPage (Pattern& p, int track, int lane, int page)
{
    auto all = allLocks (p);
    all.erase (std::remove_if (all.begin(), all.end(), [&] (const LockRef& a) { return a.track == track && a.lane == lane && a.step / 16 == page; }), all.end());
    for (int i = 0; i < 16; ++i) p.t[(size_t) track].lane[(size_t) lane][(size_t) (page * 16 + i)] = Step();
    rebuildPool (p, std::move (all));
}
inline void pastePage (Pattern& p, int track, int lane, int page, const PageClip& c)
{
    if (! c.valid) return;
    clearPage (p, track, lane, page);
    for (int i = 0; i < 16; ++i) p.t[(size_t) track].lane[(size_t) lane][(size_t) (page * 16 + i)] = c.steps[(size_t) i];
    auto all = allLocks (p);
    for (const auto& a : c.locks) all.push_back ({ track, lane, page * 16 + a.step, a.id, a.v });
    rebuildPool (p, std::move (all));
}

} // namespace dh::dv::stepedit
