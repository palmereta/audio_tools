#pragma once
// DeVicenti: the sequencer (plan §5.5, §13; genre-recipes §8). No JUCE.
//
// Data: 16 patterns, each with a master length (1-64 sixteenths), a scale (root + one of 19 scales; the first 16 are
// Orrery's list, so chord follow agrees), a swing (50-80 %) and four tracks. A track has a length (1-64), a speed
// (1/8 … 2x: polymeter) and a swing amount, and eight lanes of 64 steps (TONE and MIDI tracks use lane 0; a KIT
// track uses all eight, each lane may have its own length). A step holds a trig (note or lock-only), up to eight
// notes or a chord (scale degree or chord memory), velocity, length (with tie and slide), micro timing (±½ step in
// 1/48 steps), a retrig (rate, count, velocity ramp), a condition and a probability, an optional patch swap and up to
// 255 step locks stored in the pattern's lock pool.
//
// Step conditions: probability %, A:B (a-th of every b loops of the track or lane), FILL / !FILL, PRE / !PRE (the
// previous condition on this lane), NEI / !NEI (the latest condition on the neighbour: previous lane, or previous
// track for lane 0), 1ST / !1ST, and two of ours: LAST / !LAST (the last loop before a queued pattern change) and
// BAR n/m (bar n of every m bars of the host timeline, independent of the track length).
//
// Time: everything in quarter notes (ppq) of the host timeline. Playback is a chain of segments, one pass of a
// pattern each (Pallor's model): the segment at a position is a pure function of the position and the settings, so
// a host jump recomputes from there; while playing, the next segment is decided half a beat before it starts with
// the settings of that moment (a pattern change waits for the end of the pattern; a direct jump starts a new chain
// at once). Consecutive block windows are chained (the new window starts where the previous one ended, even if the
// host leaves a rounding gap), so no step is lost or doubled at a block boundary (AT303's T2b lesson). Inside a
// window the candidate steps are evaluated in time order (then track, then lane), which keeps PRE/NEI chains the
// same with any block size; probabilities are hashes of (seed, pattern pass, track, lane, step).

#include "Base.h"
#include <vector>

namespace dh::dv {

constexpr int kTracks = 4, kLanes = 8, kMaxSteps = 64, kPatterns = 16, kMaxChain = 64, kChordMems = 8, kMaxNotes = 8, kPoolLocks = 1024;

enum Machine : int { kTone = 0, kKit = 1, kMidiMachine = 2 };

enum Cond : uint8_t { cNone, cFill, cNotFill, cPre, cNotPre, cNei, cNotNei, cFirst, cNotFirst, cLast, cNotLast, cAB, cBar, kNumConds };
inline const char* condName (int c)
{
    static const char* n[kNumConds] = { "None", "FILL", "!FILL", "PRE", "!PRE", "NEI", "!NEI", "1ST", "!1ST", "LAST", "!LAST", "A:B", "BAR" };
    return n[c < 0 || c >= kNumConds ? 0 : c];
}

enum ChordKind : uint8_t { chOff = 0, chDegree = 1, chMemory = 2 };
enum ChordType : uint8_t { ctDiatonic, ctSus2, ctSus4, ctAdd9, ctQuartal, ctCluster, ctFifths, ctShell, ctPower, kNumChordTypes };
inline const char* chordTypeName (int t) { static const char* n[kNumChordTypes] = { "Diatonic", "Sus2", "Sus4", "Add9", "Quartal", "Cluster", "Open fifths", "Shell", "Power" }; return n[t < 0 || t >= kNumChordTypes ? 0 : t]; }
enum Voicing : uint8_t { vcClose, vcDrop2, vcDrop3, vcSpread, vcNoRoot, kNumVoicings };
inline const char* voicingName (int v) { static const char* n[kNumVoicings] = { "Close", "Drop-2", "Drop-3", "Spread", "No root" }; return n[v < 0 || v >= kNumVoicings ? 0 : v]; }
enum ChordFlag : uint8_t { cfRootBass = 1, cfFlat9 = 2, cfSharp11 = 4, cfFlat5 = 8 };
enum StepFlag : uint8_t { sfSlide = 1 };

struct Step
{
    uint8_t trig = 0;               // 0 none, 1 note trig, 2 lock-only trig
    uint8_t nNotes = 0;             // 0: the track's default note (TONE 60, KIT lane pitch)
    int8_t notes[kMaxNotes] {};     // MIDI notes; chord degree: notes[0] = degree (signed), notes[1] = octave (1..7)
    uint8_t vel = 100;
    uint16_t len = 12;              // in 1/12 of a step; kTie = held until the next trig of the lane
    int8_t micro = 0;               // −24..24, in 1/48 of a step
    uint8_t retrig = 0;             // 0 off, 1..5 = 1/2, 1/3, 1/4, 1/6, 1/8 of a step
    uint8_t retrigCount = 2;        // 2..16
    int8_t retrigRamp = 0;          // velocity change over the roll (−127..127)
    uint8_t cond = cNone, condA = 1, condB = 2, prob = 100;
    uint8_t chord = chOff, chordType = ctDiatonic, chordSize = 3, voicing = vcClose, inversion = 0, chordFlags = 0;
    int8_t patch = -1;              // patch swap: index into the project's patch pool
    uint8_t flags = 0;              // sfSlide
    uint8_t lockCount = 0;
    uint16_t lockStart = 0;
    static constexpr uint16_t kTie = 0xFFFF;
    bool on() const { return trig != 0; }
};

struct PoolLock { int16_t id = -1; float v = 0.0f; };

struct TrackPat
{
    uint8_t length = 16, speed = 4, swing = 100, pad = 0;     // speed index into speedMul(); swing: % of the pattern swing
    std::array<uint8_t, kLanes> laneLen {};                   // KIT: 0 = the track length
    std::array<std::array<Step, kMaxSteps>, kLanes> lane {};
};
inline double speedMul (int i) { static const double m[7] = { 0.125, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 }; return m[i < 0 ? 0 : (i > 6 ? 6 : i)]; }
inline const char* speedName (int i) { static const char* n[7] = { "1/8x", "1/4x", "1/2x", "3/4x", "1x", "3/2x", "2x" }; return n[i < 0 ? 0 : (i > 6 ? 6 : i)]; }

struct Pattern
{
    uint8_t length = 16, root = 9, scale = 1, swing = 50;      // root pitch class (9 = A), scale index, swing %
    std::array<TrackPat, kTracks> t {};
    std::array<PoolLock, kPoolLocks> pool {};
    uint16_t nLocks = 0;
    bool isEmpty() const { for (const auto& tr : t) for (const auto& ln : tr.lane) for (const auto& s : ln) if (s.on()) return false; return true; }
};

struct ChainEntry { uint8_t pattern = 0, repeats = 1; bool fill = false; uint8_t muteMask = 0; };

struct ChordMem { uint8_t n = 0; int8_t iv[kMaxNotes] {}; };

struct Bank
{
    std::array<Pattern, kPatterns> p {};
    std::array<ChainEntry, kMaxChain> chain {};
    int chainLen = 0;
};

// ───────────── scales and chords ─────────────
struct ScaleDef { const char* name; int n; int s[12]; };
inline const std::array<ScaleDef, 19>& scales()
{
    static const std::array<ScaleDef, 19> s {{
        { "Major (Ionian)", 7, { 0, 2, 4, 5, 7, 9, 11 } }, { "Natural minor", 7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorian", 7, { 0, 2, 3, 5, 7, 9, 10 } }, { "Phrygian", 7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian", 7, { 0, 2, 4, 6, 7, 9, 11 } }, { "Mixolydian", 7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian", 7, { 0, 1, 3, 5, 6, 8, 10 } }, { "Harmonic minor", 7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "Phrygian dominant", 7, { 0, 1, 4, 5, 7, 8, 10 } }, { "Double harmonic", 7, { 0, 1, 4, 5, 7, 8, 11 } },
        { "Major pentatonic", 5, { 0, 2, 4, 7, 9 } }, { "Minor pentatonic", 5, { 0, 3, 5, 7, 10 } },
        { "Hirajoshi", 5, { 0, 2, 3, 7, 8 } }, { "In-sen", 5, { 0, 1, 5, 7, 10 } },
        { "Whole tone", 6, { 0, 2, 4, 6, 8, 10 } }, { "Lydian augmented", 7, { 0, 2, 4, 6, 8, 9, 11 } },
        { "Melodic minor", 7, { 0, 2, 3, 5, 7, 9, 11 } }, { "Diminished", 8, { 0, 2, 3, 5, 6, 8, 9, 11 } },
        { "Chromatic", 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    }};
    return s;
}
inline const ScaleDef& scaleOf (int i) { return scales()[(size_t) std::clamp (i, 0, (int) scales().size() - 1)]; }

inline int floorDiv (int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
// MIDI note of scale degree `deg` (0 = root) above `base` (a MIDI note on the root).
inline int degreeNote (int base, const ScaleDef& sc, int deg)
{
    const int o = floorDiv (deg, sc.n), k = deg - o * sc.n;
    return base + 12 * o + sc.s[k];
}
inline int quantizeToScale (int note, int rootPc, const ScaleDef& sc)
{
    const int pc = ((note - rootPc) % 12 + 12) % 12;
    int best = 0, bd = 99;
    for (int i = 0; i < sc.n; ++i) { int d = std::abs (sc.s[i] - pc); d = std::min (d, 12 - d); if (d < bd) { bd = d; best = sc.s[i]; } }
    int diff = best - pc; if (diff > 6) diff -= 12; if (diff < -6) diff += 12;
    return note + diff;
}

// Build the notes of a chord step. Returns the count (≤ 12) in out[].
inline int buildChord (const Step& st, int rootPc, int scaleIdx, const std::array<ChordMem, kChordMems>& mem, int* out)
{
    const ScaleDef& sc = scaleOf (scaleIdx);
    int n = 0, tmp[16];
    int rootNote = 0;
    if (st.chord == chMemory)
    {
        const ChordMem& m = mem[(size_t) std::clamp ((int) st.chordType, 0, kChordMems - 1)];
        rootNote = st.notes[0];
        if (m.n == 0) { tmp[n++] = rootNote; }
        for (int i = 0; i < m.n && n < 12; ++i) tmp[n++] = quantizeToScale (rootNote + m.iv[i], rootPc, sc);
    }
    else
    {
        const int deg = st.notes[0], oct = std::clamp ((int) (st.nNotes > 1 ? st.notes[1] : 4), 0, 8);
        const int base = 12 * (oct + 1) + rootPc;
        rootNote = degreeNote (base, sc, deg);
        auto D = [&] (int k) { return degreeNote (base, sc, deg + k); };
        const int size = std::clamp ((int) st.chordSize, 2, 7);
        switch (st.chordType)
        {
            case ctSus2: tmp[n++] = D (0); tmp[n++] = D (1); tmp[n++] = D (4); if (size >= 4) tmp[n++] = D (6); break;
            case ctSus4: tmp[n++] = D (0); tmp[n++] = D (3); tmp[n++] = D (4); if (size >= 4) tmp[n++] = D (6); break;
            case ctAdd9: tmp[n++] = D (0); tmp[n++] = D (2); tmp[n++] = D (4); tmp[n++] = D (8); break;
            case ctQuartal: for (int k = 0; k < std::max (3, size); ++k) tmp[n++] = D (3 * k); break;
            case ctCluster: tmp[n++] = rootNote; tmp[n++] = rootNote + 1; tmp[n++] = rootNote + 7; if (size >= 4) tmp[n++] = rootNote + 8; break;
            case ctFifths: tmp[n++] = rootNote; tmp[n++] = rootNote + 7; tmp[n++] = rootNote + 14; if (size >= 4) tmp[n++] = rootNote + 21; break;
            case ctShell: tmp[n++] = D (0); tmp[n++] = D (2); tmp[n++] = D (6); break;
            case ctPower: tmp[n++] = rootNote; tmp[n++] = rootNote + 7; tmp[n++] = rootNote + 12; break;
            default:
                if (size == 2) { tmp[n++] = D (0); tmp[n++] = D (4); }
                else for (int k = 0; k < size; ++k) tmp[n++] = D (2 * k);
                break;
        }
        if (st.chordFlags & cfFlat9) tmp[n++] = rootNote + 13;
        if (st.chordFlags & cfSharp11) tmp[n++] = rootNote + 18;
        if (st.chordFlags & cfFlat5) for (int i = 0; i < n; ++i) if (tmp[i] - rootNote == 7) tmp[i] = rootNote + 6;
    }
    std::sort (tmp, tmp + n);
    for (int k = 0; k < st.inversion && n > 1; ++k) { const int lo = tmp[0]; for (int i = 0; i + 1 < n; ++i) tmp[i] = tmp[i + 1]; tmp[n - 1] = lo + 12; }
    std::sort (tmp, tmp + n);
    switch (st.voicing)
    {
        case vcDrop2: if (n >= 3) { tmp[n - 2] -= 12; std::sort (tmp, tmp + n); } break;
        case vcDrop3: if (n >= 4) { tmp[n - 3] -= 12; std::sort (tmp, tmp + n); } break;
        case vcSpread: for (int i = 1; i < n; i += 2) tmp[i] += 12; std::sort (tmp, tmp + n); break;
        case vcNoRoot: { int m = 0; for (int i = 0; i < n; ++i) if (((tmp[i] - rootNote) % 12 + 12) % 12 != 0 || n <= 2) tmp[m++] = tmp[i]; n = m; break; }
        default: break;
    }
    int k = 0;
    if (st.chordFlags & cfRootBass) out[k++] = rootNote - 12;
    for (int i = 0; i < n && k < 12; ++i) out[k++] = std::clamp (tmp[i], 0, 127);
    return k;
}

// ───────────── sequencer ─────────────
struct SeqSettings
{
    int pattern = 0;            // requested pattern (a change waits for the end of the current pass)
    bool song = false;          // play the chain
    bool fill = false;
    uint32_t seed = 1;
    int rootOverride = -1, scaleOverride = -1;   // harmony follow
};

struct Segment
{
    double start = 0, end = 0, entryStart = 0;
    int pat = 0, iter = 0, entry = -1, rep = 0, len = 16;
    int64_t k = 0; bool fill = false, song = false, last = false; uint8_t muteMask = 0;
};

// One step instance produced for a window.
struct SeqEvent
{
    double ppq = 0, gateBeats = 0;
    int track = 0, lane = 0, stepIdx = 0, pat = 0, hit = 0;    // hit: index inside a retrig roll
    float vel = 0.8f;
    const Step* step = nullptr;
    uint64_t key = 0;                                          // hash key of this instance (humanise, strum alternation)
};

inline double swingWarp (double ppq, float swing)
{
    if (swing <= 0.5001f) return ppq;
    const double pl = 0.5, u = ppq / pl, fl = std::floor (u), f = u - fl, s = (double) clampf (swing, 0.5f, 0.8f);
    const double w = f < 0.5 ? f * 2.0 * s : s + (f - 0.5) * 2.0 * (1.0 - s);
    return (fl + w) * pl;
}

class Sequencer
{
public:
    static constexpr double kStepBeats = 0.25;
    void setBank (const Bank* b) { bank = b; }
    void reset() { nSeg = 0; haveLast = false; jumpPending = false; for (auto& t : pre) t.fill (true); nei.fill (false); }

    // Direct jump: the next window starts a new chain at its first position with pattern p.
    void jumpTo (int p) { jumpPattern = std::clamp (p, 0, kPatterns - 1); jumpPending = true; }

    template <typename F> void advance (double c0, int n, double rate, const SeqSettings& st, F&& emit)
    {
        const double tol = std::max (2.0 * rate, 1e-7);
        double w0;
        if (haveLast && std::fabs (c0 - lastC1) <= tol) w0 = lastW1;
        else { nSeg = 0; w0 = c0 - 0.5 * rate; }
        const double w1 = c0 + ((double) n - 0.5) * rate;
        haveLast = true; lastC1 = c0 + (double) n * rate; lastW1 = w1;
        if (jumpPending)
        {
            jumpPending = false;
            Segment s; s.start = std::max (0.0, c0); s.entryStart = s.start; s.pat = jumpPattern; s.song = false; s.k = 0;
            s.end = s.start + patternBeats (s.pat); finalize (s, st);
            head = 0; seg[0] = s; nSeg = 1;
            w0 = c0 - 0.5 * rate;
        }
        window (w0, w1, st, emit);
    }
    template <typename F> void collect (double w0, double w1, const SeqSettings& st, F&& emit) { nSeg = 0; window (w0, w1, st, emit); nSeg = 0; haveLast = false; }

    const Segment* segmentAt (double ppq) const
    {
        for (int i = 0; i < nSeg; ++i) { const auto& s = seg[(size_t) ((head + i) % kRing)]; if (ppq >= s.start && ppq < s.end) return &s; }
        return nullptr;
    }
    int stepOf (const Segment& s, int track, int lane, double ppq) const
    {
        if (bank == nullptr) return -1;
        const auto& tp = bank->p[(size_t) s.pat].t[(size_t) track];
        const double sb = kStepBeats / speedMul (tp.speed);
        const int L = laneLength (tp, lane);
        const int64_t g = std::llround ((s.start - s.entryStart) / sb) + (int64_t) std::floor ((ppq - s.start) / sb);
        return (int) (((g % L) + L) % L);
    }
    double patternBeats (int pat) const { return std::max (1, (int) bank->p[(size_t) pat].length) * kStepBeats; }
    static int laneLength (const TrackPat& tp, int lane) { const int l = tp.laneLen[(size_t) lane] ? tp.laneLen[(size_t) lane] : tp.length; return std::clamp (l, 1, kMaxSteps); }

private:
    static constexpr int kRing = 32, kCand = 1024;
    const Bank* bank = nullptr;
    std::array<Segment, kRing> seg {}; int head = 0, nSeg = 0;
    bool haveLast = false, jumpPending = false; int jumpPattern = 0; double lastC1 = 0, lastW1 = 0;
    std::array<std::array<bool, kLanes>, kTracks> pre {};
    std::array<bool, kTracks * kLanes> nei {};
    struct Cand { double nominal, t; int track, lane, idx, seg; int64_t g; };
    std::array<Cand, kCand> cand {};

    Segment& at (int i) { return seg[(size_t) ((head + i) % kRing)]; }

    void finalize (Segment& s, const SeqSettings& st) const
    {
        s.len = std::max (1, (int) std::ceil ((s.end - s.start) / kStepBeats - 1e-9));
        if (s.song) s.last = false;      // set by next() when known
        else s.last = st.pattern != s.pat;
    }

    bool songOn (const SeqSettings& st) const { return st.song && bank->chainLen > 0; }

    Segment pure (double x, const SeqSettings& st) const
    {
        Segment s;
        if (! songOn (st))
        {
            s.pat = std::clamp (st.pattern, 0, kPatterns - 1);
            const double d = patternBeats (s.pat);
            s.k = (int64_t) std::floor (x / d);
            s.start = (double) s.k * d; s.end = s.start + d; s.entryStart = 0; s.iter = (int) s.k; s.fill = st.fill;
        }
        else
        {
            double total = 0; int reps = 0;
            for (int e = 0; e < bank->chainLen; ++e) { const auto& ce = bank->chain[(size_t) e]; total += std::max (1, (int) ce.repeats) * patternBeats (ce.pattern); reps += std::max (1, (int) ce.repeats); }
            const int64_t c = (int64_t) std::floor (x / total);
            double r = x - (double) c * total, acc = (double) c * total; int64_t kk = c * reps;
            for (int e = 0; e < bank->chainLen; ++e)
            {
                const auto& ce = bank->chain[(size_t) e];
                const int rp = std::max (1, (int) ce.repeats);
                const double d = patternBeats (ce.pattern);
                if (r < d * rp || e == bank->chainLen - 1)
                {
                    const int rep = std::clamp ((int) std::floor (r / d), 0, rp - 1);
                    s.song = true; s.entry = e; s.rep = rep; s.pat = ce.pattern; s.muteMask = ce.muteMask;
                    s.entryStart = acc; s.start = acc + rep * d; s.end = s.start + d; s.iter = rep; s.k = kk + rep;
                    s.fill = (ce.fill && rep == rp - 1) || st.fill;
                    s.last = rep == rp - 1 && bank->chain[(size_t) ((e + 1) % bank->chainLen)].pattern != ce.pattern;
                    break;
                }
                r -= d * rp; acc += d * rp; kk += rp;
            }
        }
        if (! s.song) finalize (s, st);
        else s.len = std::max (1, (int) std::ceil ((s.end - s.start) / kStepBeats - 1e-9));
        return s;
    }

    Segment next (const Segment& p, const SeqSettings& st) const
    {
        Segment s; s.start = p.end; s.k = p.k + 1;
        if (! songOn (st))
        {
            s.pat = std::clamp (st.pattern, 0, kPatterns - 1);
            const bool same = p.pat == s.pat && ! p.song;
            s.entryStart = same ? p.entryStart : s.start; s.iter = same ? p.iter + 1 : 0;
            s.end = s.start + patternBeats (s.pat); s.fill = st.fill;
            finalize (s, st);
        }
        else
        {
            int e = 0, rep = 0;
            if (p.song && p.entry >= 0 && p.entry < bank->chainLen)
            {
                e = p.entry; rep = p.rep + 1;
                if (rep >= std::max (1, (int) bank->chain[(size_t) e].repeats)) { e = (e + 1) % bank->chainLen; rep = 0; }
            }
            const auto& ce = bank->chain[(size_t) e];
            const int rp = std::max (1, (int) ce.repeats);
            s.song = true; s.entry = e; s.rep = rep; s.pat = ce.pattern; s.muteMask = ce.muteMask;
            const bool sameRun = p.song && p.pat == ce.pattern;
            s.entryStart = (rep == 0 && ! sameRun) ? s.start : p.entryStart; s.iter = sameRun ? p.iter + 1 : rep;
            s.end = s.start + patternBeats (s.pat);
            s.fill = (ce.fill && rep == rp - 1) || st.fill;
            s.last = rep == rp - 1 && bank->chain[(size_t) ((e + 1) % bank->chainLen)].pattern != ce.pattern;
            s.len = std::max (1, (int) std::ceil ((s.end - s.start) / kStepBeats - 1e-9));
        }
        return s;
    }

    template <typename F> void window (double w0, double w1, const SeqSettings& st, F&& emit)
    {
        if (bank == nullptr || w1 <= w0) return;
        const double ahead = 0.5;
        if (nSeg == 0) { head = 0; seg[0] = pure (w0, st); nSeg = 1; }
        while (at (nSeg - 1).end < w1 + ahead)
        {
            const Segment nx = next (at (nSeg - 1), st);
            if (nSeg == kRing) { head = (head + 1) % kRing; --nSeg; }
            at (nSeg) = nx; ++nSeg;
        }
        // The current segment's "last" flag follows the settings of now (a queued change marks the running pass).
        for (int i = 0; i < nSeg; ++i) if (! at (i).song) at (i).last = (i + 1 < nSeg) ? at (i + 1).pat != at (i).pat : st.pattern != at (i).pat;
        int nc = 0;
        for (int i = 0; i < nSeg; ++i) gather (at (i), i, w0, w1, st, nc);
        std::sort (cand.begin(), cand.begin() + nc, [] (const Cand& a, const Cand& b) {
            if (a.t != b.t) return a.t < b.t;
            if (a.track != b.track) return a.track < b.track;
            return a.lane < b.lane; });
        for (int c = 0; c < nc; ++c) fire (cand[(size_t) c], st, w0, w1, emit);
        while (nSeg > 1 && at (0).end < w0 - 4.0) { head = (head + 1) % kRing; --nSeg; }
    }

    // Candidate steps whose (possibly shifted) time can fall inside [w0, w1).
    void gather (const Segment& s, int segIdx, double w0, double w1, const SeqSettings& st, int& nc)
    {
        const Pattern& pt = bank->p[(size_t) s.pat];
        const float swing = clampf ((float) pt.swing * 0.01f, 0.5f, 0.8f);
        for (int tr = 0; tr < kTracks; ++tr)
        {
            if (s.muteMask & (1 << tr)) continue;
            const TrackPat& tp = pt.t[(size_t) tr];
            const double sb = kStepBeats / speedMul (tp.speed);
            const double late = 1.5 * sb + 0.05, early = 0.5 * sb + 0.05;     // micro ±½ step + swing
            const double lo = std::max (s.start, w0 - late), hi = std::min (s.end, w1 + early);
            if (hi <= lo) continue;
            const int i0 = std::max (0, (int) std::ceil ((lo - s.start) / sb - 1e-9));
            const int nSteps = std::max (1, (int) std::ceil ((s.end - s.start) / sb - 1e-9));
            const int i1 = std::min (nSteps, (int) std::ceil ((hi - s.start) / sb - 1e-9));
            const int64_t off = std::llround ((s.start - s.entryStart) / sb);
            for (int lane = 0; lane < kLanes; ++lane)
            {
                const auto& steps = tp.lane[(size_t) lane];
                const int L = laneLength (tp, lane);
                for (int i = i0; i < i1; ++i)
                {
                    const int64_t g = off + i;
                    const int idx = (int) (((g % L) + L) % L);
                    const Step& sp = steps[(size_t) idx];
                    if (! sp.on()) continue;
                    const double nominal = s.start + (double) i * sb;
                    double t = nominal;
                    const float amt = (float) tp.swing * 0.01f;
                    if (amt > 0.0f && swing > 0.5001f) t += amt * (swingWarp (nominal, swing) - nominal);
                    t += (double) sp.micro / 48.0 * sb;
                    if (nc < kCand) cand[(size_t) nc++] = { nominal, t, tr, lane, idx, segIdx, g };
                }
            }
        }
        (void) st;
    }

    template <typename F> void fire (const Cand& c, const SeqSettings& st, double w0, double w1, F&& emit)
    {
        const Segment& s = at (c.seg);
        const Pattern& pt = bank->p[(size_t) s.pat];
        const TrackPat& tp = pt.t[(size_t) c.track];
        const Step& sp = tp.lane[(size_t) c.lane][(size_t) c.idx];
        const double sb = kStepBeats / speedMul (tp.speed);
        const int L = laneLength (tp, c.lane);
        const int loop = (int) (c.g >= 0 ? c.g / L : (c.g - L + 1) / L);
        // Retrig roll: when does it reach into this window? Conditions are evaluated once, at the first hit.
        const int hits = sp.retrig ? std::clamp ((int) sp.retrigCount, 2, 16) : 1;
        static const double rr[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const double rstep = sp.retrig ? rr[std::clamp ((int) sp.retrig, 1, 5)] * sb : 0.0;
        if (! (c.t >= w0 && c.t < w1)) return;
        const uint64_t key = mix64 ((uint64_t) s.k * 131 + (uint64_t) c.track * 17 + (uint64_t) c.lane * 3 + (uint64_t) c.idx * 7919 + ((uint64_t) s.pat << 40));
        bool pass = true; bool conditional = false;
        if (sp.prob < 100) { conditional = true; pass = hashUni (st.seed, key, 99) * 100.0 < (double) sp.prob; }
        bool& myPre = pre[(size_t) c.track][(size_t) c.lane];
        const int neiIdx = c.lane > 0 ? c.track * kLanes + c.lane - 1 : ((c.track + kTracks - 1) % kTracks) * kLanes;
        bool res = true; bool isChain = false;
        switch (sp.cond)
        {
            case cFill: res = s.fill || st.fill; break;
            case cNotFill: res = ! (s.fill || st.fill); break;
            case cPre: res = myPre; isChain = true; break;
            case cNotPre: res = ! myPre; isChain = true; break;
            case cNei: res = nei[(size_t) neiIdx]; isChain = true; break;
            case cNotNei: res = ! nei[(size_t) neiIdx]; isChain = true; break;
            case cFirst: res = s.iter == 0; break;
            case cNotFirst: res = s.iter != 0; break;
            case cLast: res = s.last; break;
            case cNotLast: res = ! s.last; break;
            case cAB: { const int b = std::max (1, (int) sp.condB), a = std::clamp ((int) sp.condA, 1, b); res = ((loop % b) + b) % b == a - 1; break; }
            case cBar: { const int b = std::max (1, (int) sp.condB), a = std::clamp ((int) sp.condA, 1, b); const int64_t bar = (int64_t) std::floor (c.nominal / 4.0 + 1e-9); res = ((bar % b) + b) % b == a - 1; break; }
            default: break;
        }
        if (sp.cond != cNone) conditional = true;
        pass = pass && res;
        if (conditional && ! isChain) { myPre = pass; nei[(size_t) (c.track * kLanes + c.lane)] = pass; }
        else if (conditional && isChain) { nei[(size_t) (c.track * kLanes + c.lane)] = pass; }
        if (! pass) return;
        double gate = sp.len == Step::kTie ? 1.0e6 : (double) sp.len / 12.0 * sb;
        if (sp.flags & sfSlide) gate += sb * 1.05;
        for (int h = 0; h < hits; ++h)
        {
            const double th = c.t + h * rstep;
            if (h > 0 && ! (th >= w0 && th < w1)) { if (th >= w1) pendingRoll (c, sp, s, sb, rstep, hits, h, key); break; }
            SeqEvent e;
            e.ppq = th; e.track = c.track; e.lane = c.lane; e.stepIdx = c.idx; e.pat = s.pat; e.hit = h; e.step = &sp; e.key = key;
            const float v0 = (float) sp.vel, v1 = (float) std::clamp ((int) sp.vel + (int) sp.retrigRamp, 1, 127);
            e.vel = clampf ((hits > 1 ? v0 + (v1 - v0) * (float) h / (float) (hits - 1) : v0) / 127.0f, 0.01f, 1.0f);
            e.gateBeats = hits > 1 ? std::min (gate, rstep * 0.6) : gate;
            emit (e);
        }
    }

    // Roll hits that fall into later windows: remembered (they passed their condition at the first hit).
    struct Roll { Cand c; const Step* sp; int pat; double sb, rstep; int hits, next; uint64_t key; bool active; };
    std::array<Roll, 32> rolls {};
    void pendingRoll (const Cand& c, const Step& sp, const Segment& s, double sb, double rstep, int hits, int nextHit, uint64_t key)
    {
        for (auto& r : rolls) if (! r.active) { r = { c, &sp, s.pat, sb, rstep, hits, nextHit, key, true }; return; }
    }
public:
    // Called by the engine for each window after advance(): emits remembered roll hits that fall in [w0, w1).
    template <typename F> void flushRolls (double c0, int n, double rate, F&& emit)
    {
        const double w0 = c0 - 0.5 * rate, w1 = c0 + ((double) n - 0.5) * rate;
        for (auto& r : rolls)
        {
            if (! r.active) continue;
            while (r.next < r.hits)
            {
                const double th = r.c.t + r.next * r.rstep;
                if (th < w0 - 1.0) { r.next = r.hits; break; }          // stale (a jump)
                if (th >= w1) break;
                SeqEvent e;
                e.ppq = th; e.track = r.c.track; e.lane = r.c.lane; e.stepIdx = r.c.idx; e.pat = r.pat; e.hit = r.next; e.step = r.sp; e.key = r.key;
                const float v0 = (float) r.sp->vel, v1 = (float) std::clamp ((int) r.sp->vel + (int) r.sp->retrigRamp, 1, 127);
                e.vel = clampf ((v0 + (v1 - v0) * (float) r.next / (float) (r.hits - 1)) / 127.0f, 0.01f, 1.0f);
                const double gate = r.sp->len == Step::kTie ? 1.0e6 : (double) r.sp->len / 12.0 * r.sb;
                e.gateBeats = std::min (gate, r.rstep * 0.6);
                emit (e);
                ++r.next;
            }
            if (r.next >= r.hits) r.active = false;
        }
    }
    void clearRolls() { for (auto& r : rolls) r.active = false; }
};

// Euclidean fill (Bjorklund by the bresenham form): pulses over steps, rotated. Writes trigs into a lane.
inline void euclid (std::array<Step, kMaxSteps>& lane, int pulses, int steps, int rotation, uint8_t vel = 100)
{
    steps = std::clamp (steps, 1, kMaxSteps); pulses = std::clamp (pulses, 0, steps);
    for (int i = 0; i < steps; ++i)
    {
        const int j = ((i - rotation) % steps + steps) % steps;
        const bool hit = pulses > 0 && ((j * pulses) % steps) < pulses;
        lane[(size_t) i].trig = hit ? 1 : 0;
        if (hit) lane[(size_t) i].vel = vel;
    }
}

} // namespace dh::dv
