#pragma once
// DeviceNTI V: the engine (plan §5, §13), no JUCE: the plugin, devicentirender and devicentitest all use it.
//
// Per block: the sequencer gives the steps of the block (sample-exact, chained windows), MIDI input is mapped to
// tracks (channels 1-4 or OMNI to the focused track; KIT lanes by a GM-like map), both become timed note starts in
// one queue (strum and humanise delays included), and the block is rendered in segments that never cross an event
// or a 16-sample control boundary of the absolute timeline, so the output is the same for any block size.
//
// Voices: 16 sound at once, from a pool of 24 slots (the extra slots let a stolen voice finish its 5 ms fade while the
// new note starts). Stealing takes released voices first (quietest), then the oldest held one; a track's own voice
// limit is enforced first. KIT lanes are mono, with choke groups. MIDI-machine tracks play no voices: their steps,
// chords and step locks (as CC) go to MIDI out; TONE and KIT tracks can double their notes to MIDI out.
//
// Track bus: FM layer → (KIT: bus filter and drive) → insert → AGE tape → sends → + sub → low-end mono → fader and
// duck → main or the track's own output. Sends return through DELAY and REVERB (delay into reverb), then the master.

#include "Voice.h"
#include "Bus.h"
#include "Sends.h"
#include "Master.h"
#include "Seq.h"
#include <memory>
#include <vector>

namespace dh::dv {

constexpr int kSlots = 24, kMaxActive = 16, kSoundsPerTrack = 9, kPatches = 16;

// ───────────── project: everything that is state (not host parameters) ─────────────
struct TrackSettings
{
    int machine = kTone;
    bool mute = false, solo = false;
    int inChannel = 1, outChannel = 1;     // MIDI in (per-channel mode) and MIDI out channels
    bool doubleMidi = false;               // TONE/KIT: also send the notes to MIDI out
    int outBus = 0;                        // 0 main, 1 the track's own output (when the host enables it)
    bool chordPlay = false; int chordPlaySize = 3;   // MIDI in / keys play diatonic chords
};

struct FxSettings
{
    int delayMode = kDelayTape; float delayTone = 0.55f, delayWow = 0.2f, delayReturn = 0.8f, delayToReverb = 0.2f;
    int reverbMode = kRevHall; float reverbDamp = 0.5f, reverbPredelay = 0.015f, reverbMod = 0.3f, reverbShimmer = 0.0f, reverbReturn = 0.8f;
    bool delayFreeze = false, reverbFreeze = false;
    float tapeNoise = 0.3f, duckRelease = 0.25f;
    int duckTrack = -1;                    // −1: the first KIT track
};

struct MidiSettings
{
    int inMode = 0;                        // 0 OMNI (every channel plays the focused track), 1 channels per track
    int focus = 1;                         // the second track: in the factory projects the first is the KIT
    int ccMode = 0;                        // 0 absolute, 1 relative two's complement, 2 relative offset 64
    bool ccFeedback = false;
    int pcMode = 0;                        // 0 program change selects the pattern, 1 the preset
    int harmonyChannel = 0;                // 0 off, 1-16: held notes there set the chord root and scale
    bool clockIn = false, clockOut = true, thru = false;
    int recQuantize = 1;                   // 0 keep micro timing, 1 quantise to the step
};

struct Project
{
    std::array<TrackSettings, kTracks> track {};
    std::array<std::array<Sound, kSoundsPerTrack>, kTracks> sounds {};
    std::array<Sound, kPatches> patches {};
    std::array<ChordMem, kChordMems> chordMem {};
    Bank bank;
    FxSettings fx;
    MidiSettings midi;
    uint32_t seed = 1;
};

// Host-automatable values (the 64 parameters), read once per block.
struct TrackAuto { float cutoff = 8000, reso = 0.1f, fenv = 0, drive = 0, age = 0, dly = 0, rev = 0, levelDb = 0, op[4] { 1, 0, 0, 0 }, ins = 0.5f; };
struct EngineParams
{
    std::array<TrackAuto, kTracks> t {};
    float swing = 0.56f, masterDb = 0.0f;
    int delayDiv = 9; float delayFb = 0.4f, revSize = 0.5f, revDecay = 3.0f;
    float tape = 0.0f, glue = 0.0f, duck = 0.0f, limit = 0.0f;
    float tempo = 88.0f; int pattern = 0;
    bool fill = false, song = false, rec = false;
};

struct HostTime { bool playing = false; double ppq = 0.0, bpm = 120.0; };
struct MidiIn { int offset = 0; uint8_t d[3] {}; };                       // raw short message
struct MidiOut { int offset = 0; uint8_t d[3] {}; int size = 3; };
struct RecEvent { int track = 0, lane = 0, step = 0, note = 60, vel = 100, micro = 0, lenTicks = 12; int pattern = 0; bool isNoteOff = false; int lockId = -1; float lockValue = 0.0f; };
struct SoundEdit { int track = 0, lane = 0, param = 0; float value = 0; };

// Delay division choices of the automatable delay time (beats).
inline double delayDivBeats (int i) { static const double b[11] = { 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 3.0, 4.0 }; return b[std::clamp (i, 0, 10)]; }
inline const char* delayDivName (int i) { static const char* n[11] = { "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/2D", "1 bar" }; return n[std::clamp (i, 0, 10)]; }

// Default MIDI notes of the eight KIT lanes (GM-like) and the aliases.
inline int defaultLaneNote (int lane) { static const int n[9] = { 0, 36, 38, 39, 42, 46, 45, 50, 37 }; return n[std::clamp (lane, 0, 8)]; }

class Engine
{
public:
    Engine() { proj = std::make_unique<Project>(); seq.setBank (&proj->bank); }

    Project& project() { return *proj; }
    const Project& project() const { return *proj; }
    Sequencer& sequencer() { return seq; }

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        maxN = std::max (64, maxBlock);
        for (int s = 0; s < kSlots; ++s) { voices[(size_t) s].qs = &quadState[(size_t) (s / kQL)]; voices[(size_t) s].ql = s % kQL; voices[(size_t) s].prepare (sr, (uint32_t) (s + 1)); }
        for (auto& b : buf) b.assign ((size_t) kChunk, 0.0f);
        for (auto& tb : tbuf) for (auto& b : tb) b.assign ((size_t) kChunk, 0.0f);
        for (int t = 0; t < kTracks; ++t)
        {
            tape[(size_t) t].prepare (sr); ins[(size_t) t].prepare (sr); lowMono[(size_t) t].prepare (sr);
            subDelay[(size_t) t].prepare (64 + tape[(size_t) t].latency());
            ownDelay[(size_t) t][0].prepare (master.latency() + 512); ownDelay[(size_t) t][1].prepare (master.latency() + 512);
            kitLp[(size_t) t][0].reset(); kitLp[(size_t) t][1].reset();
        }
        delay.prepare (sr); reverb.prepare (sr); master.prepare (sr); ducker.prepare (sr);
        sendK = 1.0f - std::exp (-1.0f / (0.008f * (float) sr));
        startQ.clear(); startQ.reserve (512); offQ.clear(); offQ.reserve (1024);
        midiOutV.reserve (2048); recOut.reserve (256); soundEdits.reserve (256);
        reset();
    }

    // As freshly prepared (the host's reset(), a re-prepare): sequencer position, voices, effects, clocks.
    void reset()
    {
        for (auto& q : quadState) q = QuadState();
        for (auto& v : voices) v.reset();
        for (int t = 0; t < kTracks; ++t) { tape[(size_t) t].reset(); ins[(size_t) t].reset(); lowMono[(size_t) t].reset(); subDelay[(size_t) t].clear(); ownDelay[(size_t) t][0].clear(); ownDelay[(size_t) t][1].clear(); kitLp[(size_t) t][0].reset(); kitLp[(size_t) t][1].reset(); kitDcX[t][0] = kitDcX[t][1] = kitDcY[t][0] = kitDcY[t][1] = 0; }
        delay.reset(); reverb.reset(); master.reset(); ducker.reset();
        for (auto& ss : sendSm) ss = { -1.0f, -1.0f };
        seq.reset(); seq.clearRolls();
        startQ.clear(); offQ.clear();
        absNow = 0; serial = 0; wasPlaying = false; havePpq = false; freePpq = 0.0; jumpTol = lastHostN = lastHostRate = 0.0;
        loudT.fill (0); idleT.fill (false); loudD = loudR = 0; idleD = idleR = false; lastBusy = 0;
        for (auto& m : mod) m = TrackMod();
        for (auto& b : busTrig) b = BusTrig();
        for (auto& d : drone) d.n = 0;
        for (auto& a : arp) a = Arp();
        for (auto& g : fxLock) g = { 0.0f, -1 };
        harmonyHeld.fill (false);
        smoothInit = false;
        held.fill (0); sustainOn.fill (false);
        for (auto& v : view.step) v = -1;
    }

    int latency() const { return Voice::kLatency + tape[0].latency() + master.latency(); }

    // outs: main L/R then 4 × (track L/R); nullptr for buses the host did not enable (they go to main).
    void process (float* const* outs, int n, const HostTime& ht, const EngineParams& P, const MidiIn* midiIn, int nMidi, std::vector<MidiOut>* midiOut)
    {
        midiOutV.clear(); recOut.clear();
        { const double r = ht.bpm / 60.0 / sr; jumpTol = lastHostN * std::max (lastHostRate, r); lastHostN = n; lastHostRate = r; seq.jumpTol = jumpTol; }
        for (int i0 = 0; i0 < n; i0 += kChunk)
        {
            const int m = std::min (kChunk, n - i0);
            float* o[2 + 2 * kTracks];
            for (int k = 0; k < 2 + 2 * kTracks; ++k) o[k] = outs[k] ? outs[k] + i0 : nullptr;
            HostTime h = ht;
            if (i0 > 0) h.ppq = ht.ppq + (double) i0 * ht.bpm / 60.0 / sr;
            // MIDI of this chunk.
            int a = 0; while (a < nMidi && midiIn[a].offset < i0) ++a;
            int b = a; while (b < nMidi && midiIn[b].offset < i0 + m) ++b;
            chunkMidi.clear();
            for (int k = a; k < b; ++k) { MidiIn e = midiIn[k]; e.offset -= i0; chunkMidi.push_back (e); }
            processChunk (o, m, h, P, i0);
        }
        if (midiOut) for (auto& e : midiOutV) midiOut->push_back (e);
    }

    // After the bank changes: does any step lock the reverb freeze? (Only then the SPRING/PLATE freezer listens.)
    void refreshFreezeLocks()
    {
        bool any = false;
        for (const auto& p : proj->bank.p) for (int i = 0; i < p.nLocks && ! any; ++i) any = p.pool[(size_t) i].id == gReverbFreeze;
        freezeLocks = any;
    }

    // Sound edits from CC (the processor forwards them to the message thread to keep its copy in sync).
    void applySoundEdit (int track, int lane, int param, float value)
    {
        if (track < 0 || track >= kTracks || lane < 0 || lane >= kSoundsPerTrack || param < 0 || param >= kNumSoundParams) return;
        proj->sounds[(size_t) track][(size_t) lane][param] = clampParam (param, value);
    }

    void allNotesOff()
    {
        for (auto& v : voices) v.release();
        for (auto& e : offQ) pushMidi (0, e.d[0], e.d[1], 0);
        offQ.clear(); startQ.clear();
        for (auto& d : drone) d.n = 0;
        held.fill (0);
    }
    void allSoundOff() { for (auto& v : voices) v.kill(); allNotesOff(); }
    // MIDI notes that are still on at the output (bypass, stop): their note-offs now.
    void flushMidiOut (std::vector<MidiOut>& out) { for (auto& e : offQ) out.push_back ({ 0, { e.d[0], e.d[1], 0 }, 3 }); offQ.clear(); }

    const std::vector<RecEvent>& recordEvents() const { return recOut; }
    std::vector<SoundEdit> soundEdits;

    // For the interface and the tests.
    struct View { int pattern = -1; std::array<int, kTracks * kLanes> step {}; bool playing = false; double ppq = 0; float grDb = 0; int activeVoices = 0; } view;
    int activeVoices() const { int c = 0; for (auto& v : voices) c += v.isAlive() && ! v.isFading(); return c; }
    const Voice& voice (int i) const { return voices[(size_t) i]; }

private:
    static constexpr int kChunk = 2048;
    double sr = 48000.0; int maxN = 512;
    std::unique_ptr<Project> proj;
    Sequencer seq;
    std::array<Voice, kSlots> voices;
    std::array<QuadState, kSlots / kQL> quadState {};
    std::array<TapeStage, kTracks> tape; std::array<Insert, kTracks> ins; std::array<LowMono, kTracks> lowMono; std::array<Line, kTracks> subDelay;
    std::array<std::array<Line, 2>, kTracks> ownDelay;
    std::array<std::array<float, 2>, kTracks> sendSm {}; float sendK = 0.003f;
    std::array<std::array<Svf, 2>, kTracks> kitLp; float kitDcX[kTracks][2] {}, kitDcY[kTracks][2] {};
    Delay delay; Reverb reverb; Master master; Ducker ducker;
    // Scratch: per track fmL fmR sub dL dR rL rR; global key, sendD L/R, sendR L/R, main L/R.
    enum TB { tFmL, tFmR, tSub, tDL, tDR, tRL, tRR, kTB };
    std::array<std::array<std::vector<float>, kTB>, kTracks> tbuf;
    enum GB { gKey, gDL, gDR, gRL, gRR, gML, gMR, gDuck, kGB };
    std::array<std::vector<float>, kGB> buf;
    int64_t absNow = 0; uint32_t serial = 0;
    // Silence bypass: a track bus (kit stage, insert, tape) or a send that has had no input and an output under
    // −120 dBFS for longer than its longest internal delay stops being processed until something reaches it again.
    // Decided at control-period boundaries of the absolute timeline (the same decisions whatever the block size).
    std::array<int64_t, kTracks> loudT {}; std::array<bool, kTracks> idleT {};
    int64_t loudD = 0, loudR = 0; bool idleD = false, idleR = false;
    int64_t lastBusy = 0;                                     // last sample the main output was above −120 dBFS
    int activeSlots() const { int c = 0; for (const auto& v : voices) c += v.isAlive(); return c; }
    static int lastLoud (const float* L, const float* R, int n)
    {
        for (int i = n - 1; i >= 0; --i) if (std::fabs (L[i]) > 1.0e-6f || std::fabs (R[i]) > 1.0e-6f) return i;
        return -1;
    }
    bool wasPlaying = false, havePpq = false; double lastPpqEnd = 0, freePpq = 0.0;
    // A tempo change inside a host block (REAPER puts tempo markers anywhere) moves the next block's position away
    // from the end extrapolated with the old tempo by up to that block's length in beats: not a jump (DAW QA
    // 2026-10-25: a marker 120 -> 90 at 4.1 s cut every sounding note as if the transport had jumped).
    double jumpTol = 0.0, lastHostN = 0.0, lastHostRate = 0.0;
    bool smoothInit = false;
    std::vector<MidiIn> chunkMidi;
    std::vector<MidiOut> midiOutV;
    std::vector<RecEvent> recOut;
    std::array<uint8_t, kTracks * 128> held {};     // MIDI-in held notes per track (count)
    std::array<bool, kTracks> sustainOn {};
    std::array<bool, 128> harmonyHeld {};
    EngineParams cur;

    struct TrackMod { float wheel = 0, aftertouch = 0, bend = 0, breath = 0, expression = 0; };
    std::array<TrackMod, kTracks> mod {};
    // The latest trig of each track: its locks and patch drive the bus (sends, insert) until the next trig.
    struct BusTrig { Lock locks[kMaxLocks]; int n = 0; int patch = -1; };
    std::array<BusTrig, kTracks> busTrig {};
    // Smoothed automatable values per track (control rate).
    std::array<TrackAuto, kTracks> sm {};
    struct Drone { int n = 0; int notes[12] {}; };
    std::array<Drone, kTracks> drone {};
    // phase: the tick the arp counts from, the first one at or after the note that started it (a chord pressed on a
    // step plays its lowest note on that step: DAW QA DV5).
    struct Arp { int n = 0; int notes[16] {}; int idx = 0; int dir = 1; int64_t lastTick = INT64_MIN, phase = 0; };
    std::array<Arp, kTracks> arp {};
    bool freezeLocks = true;                 // any step of the bank locks the reverb freeze (refreshFreezeLocks)
    struct FxLock { float v; int64_t until; };
    std::array<FxLock, kGlobalLockEnd - kGlobalLockBase> fxLock {};

    struct StartEv
    {
        int64_t at = 0; int track = 0, lane = 0, note = 60, source = 0, patch = -1, gate = -1; float pitch = 60, vel = 0.8f;
        Lock locks[kMaxLocks]; int nLocks = 0; bool lockOnly = false; bool legatoOk = true; bool droneNote = false;
    };
    std::vector<StartEv> startQ;
    // Doubled tracks: the MIDI notes that went out for each key played in (track · 128 + key), so its note-off releases
    // those (DAW QA 2026-10-25: chord mode sent the key three times and one note-off; a KIT key on a GM alias got the
    // lane's note on and the key's note off; with Thru on every key went out twice).
    struct PlayedOut { uint8_t n = 0; uint8_t note[16] {}; };
    std::array<PlayedOut, kTracks * 128> playedOut {};
    struct OffEv { int64_t at; uint8_t d[2]; };
    std::vector<OffEv> offQ;

    // ── helpers ──
    void pushMidi (int offset, uint8_t status, uint8_t d1, uint8_t d2) { if (midiOutV.size() < midiOutV.capacity()) midiOutV.push_back ({ offset, { status, d1, d2 }, 3 }); }

    bool trackAudible (int t) const
    {
        bool anySolo = false; for (auto& tr : proj->track) anySolo |= tr.solo;
        const auto& tr = proj->track[(size_t) t];
        return ! tr.mute && (! anySolo || tr.solo);
    }

    int duckSource() const
    {
        if (proj->fx.duckTrack >= 0 && proj->fx.duckTrack < kTracks) return proj->fx.duckTrack;
        for (int t = 0; t < kTracks; ++t) if (proj->track[(size_t) t].machine == kKit) return t;
        return -1;
    }

    const Sound& soundFor (int t, int lane, int patch) const
    {
        if (patch >= 0 && patch < kPatches) return proj->patches[(size_t) patch];
        return proj->sounds[(size_t) t][(size_t) std::clamp (lane, 0, kSoundsPerTrack - 1)];
    }

    VoiceCtx ctxFor (int t, double ppq, double bpm, bool playing) const
    {
        VoiceCtx c; c.ppq = ppq; c.bpm = bpm; c.absSample = absNow; c.playing = playing; c.fs = sr;
        const auto& m = mod[(size_t) t];
        c.wheel = m.wheel; c.aftertouch = m.aftertouch; c.bend = m.bend; c.breath = m.breath; c.expression = m.expression;
        const Sound& s0 = proj->sounds[(size_t) t][0];
        if (proj->track[(size_t) t].machine == kKit)
        {
            c.kitDecayScale = fastExp2 (1.5f * s0[FENV]);
            c.kitAge = s0[AGE];
            for (int k = 0; k < 4; ++k) { const float v = s0[opParam (k, oLevel)] / 0.75f; c.groupGain[k] = v * v; }
        }
        return c;
    }

    // ── automation: host values into the TONE sound / KIT bus sound, smoothed at control rate ──
    void applyAutomation (bool snap)
    {
        const float k = snap ? 1.0f : 1.0f - std::exp (-(float) kCtl / (0.02f * (float) sr));
        for (int t = 0; t < kTracks; ++t)
        {
            auto& s = sm[(size_t) t]; const auto& a = cur.t[(size_t) t];
            auto lg = [&] (float& v, float target) { v = std::exp (std::log (std::max (v, 1e-3f)) + (std::log (std::max (target, 1e-3f)) - std::log (std::max (v, 1e-3f))) * k); };
            lg (s.cutoff, a.cutoff);
            s.reso += (a.reso - s.reso) * k; s.fenv += (a.fenv - s.fenv) * k; s.drive += (a.drive - s.drive) * k; s.age += (a.age - s.age) * k;
            s.dly += (a.dly - s.dly) * k; s.rev += (a.rev - s.rev) * k; s.levelDb += (a.levelDb - s.levelDb) * k; s.ins += (a.ins - s.ins) * k;
            for (int o = 0; o < 4; ++o) s.op[o] += (a.op[o] - s.op[o]) * k;
            Sound& snd = proj->sounds[(size_t) t][0];
            snd[CUTOFF] = s.cutoff; snd[RESO] = s.reso; snd[FENV] = s.fenv; snd[DRIVE] = s.drive; snd[AGE] = s.age;
            snd[SEND_DELAY] = s.dly; snd[SEND_REVERB] = s.rev; snd[INS_AMT] = s.ins;
            for (int o = 0; o < 4; ++o) snd[opParam (o, oLevel)] = s.op[o];
        }
    }

    // ── note starts ──
    int freeSlot()
    {
        // The lowest free physical slot (the compaction keeps the groups packed from slot 0).
        for (int s = 0; s < kSlots; ++s) if (! voices[(size_t) s].isAlive()) return s;
        // All slots busy (fades and flushes): the oldest, in logical order.
        int oldestSlot = 0; int64_t oldest = INT64_MAX;
        eachByLid ([&] (Voice& v, int s) { if (v.startedAt < oldest) { oldest = v.startedAt; oldestSlot = s; } });
        voices[(size_t) oldestSlot].reset(); voices[(size_t) oldestSlot].lid = -1;
        return oldestSlot;
    }
    // The logical slot of a new voice: the lowest free one (what the slot was when voices were rendered one by one).
    // It picks the filter's analog spread and the iteration order, so the sound does not depend on where the voice
    // is rendered.
    int freeLid() const
    {
        bool busy[kSlots] {};
        for (const auto& v : voices) if (v.isAlive() && v.lid >= 0) busy[v.lid] = true;
        for (int l = 0; l < kSlots; ++l) if (! busy[l]) return l;
        return 0;
    }
    Voice& allocVoice()
    {
        const int p = freeSlot();
        Voice& v = voices[(size_t) p];
        v.lid = freeLid();
        return v;
    }
    // Sounding voices in logical-slot order (where the order decides something: ties, unison indices, serials).
    template <typename F> void eachByLid (F&& f)
    {
        int idx[kSlots]; int n = 0;
        for (int s = 0; s < kSlots; ++s) if (voices[(size_t) s].isAlive() && voices[(size_t) s].lid >= 0) idx[n++] = s;
        std::sort (idx, idx + n, [&] (int a, int b) { return voices[(size_t) a].lid < voices[(size_t) b].lid; });
        for (int i = 0; i < n; ++i) f (voices[(size_t) idx[i]], idx[i]);
    }
    // At control boundaries: sounding voices move down into free slots so the groups stay packed from slot 0 (a
    // group with lanes 4-7 empty renders one register wide; an empty group not at all).
    void compactVoices()
    {
        int used = 0; for (const auto& v : voices) used += v.isAlive();
        int f = 0;
        for (int s = kSlots - 1; s >= used; --s)
        {
            if (! voices[(size_t) s].isAlive()) continue;
            while (voices[(size_t) f].isAlive()) ++f;
            Voice& src = voices[(size_t) s]; Voice& dst = voices[(size_t) f];
            QuadState* dq = dst.qs; const int dl = dst.ql;
            QuadState::moveLane (*src.qs, src.ql, *dq, dl);
            dst = src; dst.rebind (dq, dl);
            src.reset(); src.lid = -1;
        }
    }
    void stealOne (int track)
    {
        // Released first (quietest), then the oldest held; within `track` when track ≥ 0.
        int victim = -1; float bestLev = 1e9f; int64_t oldest = INT64_MAX;
        eachByLid ([&] (Voice& v, int s) {
            if (v.isFading() || (track >= 0 && v.track != track)) return;
            if (v.isReleased() && v.level() < bestLev) { bestLev = v.level(); victim = s; } });
        if (victim < 0)
            eachByLid ([&] (Voice& v, int s) {
                if (v.isFading() || (track >= 0 && v.track != track)) return;
                if (v.startedAt < oldest) { oldest = v.startedAt; victim = s; } });
        if (victim >= 0)
        {
            // Kill the whole unison group of the victim.
            const int g = voices[(size_t) victim].group, tr = voices[(size_t) victim].track;
            for (auto& v : voices) if (v.isAlive() && ! v.isFading() && v.track == tr && (g >= 0 ? v.group == g : &v == &voices[(size_t) victim])) v.kill();
        }
    }

    int samplesToBoundary (int64_t abs) const { return kCtl - (int) (abs % kCtl); }

    void startVoices (const StartEv& e, double ppq, double bpm, bool playing)
    {
        const int t = e.track;
        const auto& tr = proj->track[(size_t) t];
        if (tr.machine == kMidiMachine) return;
        const Sound& snd = soundFor (t, e.lane, e.patch);
        const VoiceCtx c = ctxFor (t, ppq, bpm, playing);
        const int stb = samplesToBoundary (absNow);
        if (tr.machine == kKit)
        {
            const int lane = std::clamp (e.lane, 1, 8);
            const int choke = snd.i (CHOKE);
            for (auto& v : voices)
            {
                if (! v.isAlive() || v.isFading() || v.track != t) continue;
                if (v.lane == lane) v.kill();
                else if (choke > 0 && proj->sounds[(size_t) t][(size_t) v.lane].i (CHOKE) == choke) v.kill();
            }
            while (activeVoices() >= kMaxActive) stealOne (-1);
            Voice::Start s; s.snd = &snd; s.track = t; s.lane = lane; s.note = e.note; s.source = e.source;
            s.pitch = snd[KIT_PITCH] + (e.pitch - 60.0f); s.velocity = e.vel; s.gateSamples = e.gate; s.seed = (uint32_t) mix64 ((uint64_t) absNow * 31 + (uint64_t) t * 7 + (uint64_t) lane);
            s.now = absNow; s.serial = ++serial; s.locks = e.locks; s.nLocks = e.nLocks; s.perVoiceSends = true; s.group = (int) serial;
            allocVoice().start (s, c, stb);
            return;
        }
        // TONE.
        const int vmode = snd.i (VMODE), uni = std::clamp (snd.i (UNISON), 1, 4);
        if (vmode > 0)
        {
            // Mono / legato: reuse the track's sounding group.
            bool any = false, anyGate = false;
            for (auto& v : voices) if (v.isAlive() && ! v.isFading() && v.track == t) { any = true; anyGate |= v.gate; }
            if (any && vmode == 2 && anyGate && e.legatoOk)
            {
                eachByLid ([&] (Voice& v, int) { if (! v.isFading() && v.track == t) v.legatoTo (e.pitch, e.note, e.gate, e.locks, e.nLocks, ++serial); });
                return;
            }
            if (any)
            {
                int k = 0; const int g = (int) ++serial;
                eachByLid ([&] (Voice& v, int) {
                    if (v.isFading() || v.track != t) return;
                    if (k >= uni) { v.kill(); return; }
                    Voice::Start s = makeStart (e, snd, t, uni, k++, g);
                    v.start (s, c, stb); });
                for (; k < uni; ++k) { while (activeVoices() >= kMaxActive) stealOne (-1); Voice::Start s = makeStart (e, snd, t, uni, k, g); allocVoice().start (s, c, stb); }
                return;
            }
        }
        // Poly: the track limit, then the pool.
        const int maxV = std::clamp (snd.i (VOICES), 1, kMaxActive);
        auto trackCount = [&] { int n = 0; for (auto& v : voices) n += v.isAlive() && ! v.isFading() && v.track == t; return n; };
        int guard = 0;
        while (trackCount() + uni > std::max (maxV, uni) && guard++ < 32) stealOne (t);
        guard = 0;
        while (activeVoices() + uni > kMaxActive && guard++ < 32) stealOne (-1);
        const int g = (int) ++serial;
        for (int k = 0; k < uni; ++k) { Voice::Start s = makeStart (e, snd, t, uni, k, g); allocVoice().start (s, c, stb); }
    }

    Voice::Start makeStart (const StartEv& e, const Sound& snd, int t, int uni, int k, int g)
    {
        Voice::Start s; s.snd = &snd; s.track = t; s.lane = 0; s.note = e.note; s.source = e.source; s.pitch = e.pitch; s.velocity = e.vel;
        s.gateSamples = e.gate; s.now = absNow; s.serial = ++serial; s.locks = e.locks; s.nLocks = e.nLocks; s.group = g;
        s.seed = (uint32_t) mix64 ((uint64_t) absNow * 131 + (uint64_t) t * 17 + (uint64_t) k * 3 + (uint64_t) e.note);
        if (uni > 1)
        {
            const float u = (float) k / (float) (uni - 1) * 2.0f - 1.0f;         // −1 … 1
            s.detuneCents = u * snd[UNI_DETUNE] * 0.5f;
            s.panOffset = u * snd[UNI_SPREAD];
            s.phaseSpread = 0.13f * (float) k;
        }
        return s;
    }

    void releaseNote (int t, int note, int source)
    {
        for (auto& v : voices) if (v.isAlive() && ! v.isFading() && v.track == t && v.note == note && v.source == source) v.release();
    }

    // ── chords / steps → starts ──
    void queueStep (const SeqEvent& e, double c0, double rate, double bpm)
    {
        const int t = e.track;
        const auto& tr = proj->track[(size_t) t];
        const Pattern& pt = proj->bank.p[(size_t) e.pat];
        const Step& sp = *e.step;
        const int64_t at0 = absNow + (int64_t) std::llround ((e.ppq - c0) / rate);
        const int gate = (int) std::min (1.0e9, std::max (1.0, e.gateBeats * 60.0 / bpm * sr));
        StartEv s; s.track = t; s.lane = tr.machine == kKit ? std::clamp (e.lane + 1, 1, 8) : 0; s.source = 0; s.patch = sp.patch; s.gate = gate;
        s.nLocks = 0;
        for (int i = 0; i < sp.lockCount && s.nLocks < kMaxLocks; ++i)
        {
            const auto& L = pt.pool[(size_t) std::min ((int) sp.lockStart + i, kPoolLocks - 1)];
            if (L.id >= kGlobalLockBase && L.id < kGlobalLockEnd) { fxLock[(size_t) (L.id - kGlobalLockBase)] = { L.v, at0 + gate }; continue; }
            if (L.id >= 0 && L.id < kNumSoundParams) s.locks[s.nLocks++] = { L.id, L.v };
        }
        // The bus follows this trig (only lane 0 / TONE: the kit bus keeps its own sound).
        if (tr.machine != kKit) { auto& b = busTrig[(size_t) t]; b.n = s.nLocks; b.patch = sp.patch; for (int i = 0; i < s.nLocks; ++i) b.locks[i] = s.locks[i]; }
        if (sp.trig == 2)
        {
            // Lock-only trig: the locks go to the sounding voices of the lane.
            s.lockOnly = true; s.at = at0; pushStart (s);
            return;
        }
        // Notes.
        int notes[16]; int nn = 0;
        const int rootPc = currentRoot (pt), scaleIdx = currentScale (pt);
        if (sp.chord != chOff) nn = buildChord (sp, rootPc, scaleIdx, proj->chordMem, notes);
        else if (sp.nNotes == 0) notes[nn++] = 60;
        else for (int i = 0; i < std::min ((int) sp.nNotes, kMaxNotes); ++i) notes[nn++] = std::clamp ((int) sp.notes[i], 0, 127);
        const Sound& snd = soundFor (t, s.lane, sp.patch);
        // Drone hold: same chord → keep it sounding; different → release the old one.
        const bool droneMode = tr.machine == kTone && snd[CHORD_HOLD] > 0.5f && sp.chord != chOff;
        if (droneMode)
        {
            auto& d = drone[(size_t) t];
            bool same = d.n == nn; for (int i = 0; same && i < nn; ++i) same = d.notes[i] == notes[i];
            if (same && e.hit == 0) return;
            for (int i = 0; i < d.n; ++i) { StartEv off; off.at = at0; off.track = t; off.note = d.notes[i]; off.source = 0; off.gate = -2; pushStart (off); }
            d.n = nn; for (int i = 0; i < nn; ++i) d.notes[i] = notes[i];
            s.gate = -1;
        }
        // Strum and humanise.
        const float strum = snd[STRUM]; const int dir = snd.i (STRUM_DIR);
        const bool down = dir == 1 || (dir == 2 && (e.key & 1));
        const float hv = snd[HUM_VEL], ht = snd[HUM_TIME];
        for (int i = 0; i < nn; ++i)
        {
            StartEv n = s;
            const int k = down ? nn - 1 - i : i;
            const double dtS = strum * (double) k + (ht > 0.0f ? 0.006 * ht * hashUni (e.key, (uint64_t) i, 41) * 2.0 : 0.0);
            n.at = at0 + (int64_t) std::llround (dtS * sr);
            n.note = notes[i]; n.pitch = (float) notes[i];
            n.vel = clampf (e.vel + (hv > 0.0f ? hv * 0.1f * hashBi (e.key, (uint64_t) i, 43) : 0.0f), 0.01f, 1.0f);
            if (n.gate > 0) n.gate = std::max (1, n.gate - (int) (n.at - at0));
            pushStart (n);
        }
    }

    int currentRoot (const Pattern& p) const { return harmonyRoot >= 0 ? harmonyRoot : p.root % 12; }
    int currentScale (const Pattern& p) const { return harmonyScale >= 0 ? harmonyScale : p.scale; }
    int harmonyRoot = -1, harmonyScale = -1;

    void pushStart (const StartEv& s)
    {
        if (startQ.size() >= 480) return;
        // Keep the queue sorted by time (stable for equal times).
        auto it = std::upper_bound (startQ.begin(), startQ.end(), s.at, [] (int64_t a, const StartEv& b) { return a < b.at; });
        startQ.insert (it, s);
    }

    void execStart (const StartEv& e, int offset, double ppq, double bpm, bool playing)
    {
        const int t = e.track;
        auto& tr = proj->track[(size_t) t];
        if (e.gate == -2)
        {
            // A drone (or MIDI) note-off.
            releaseNote (t, e.note, e.source);
            if (tr.machine == kMidiMachine || tr.doubleMidi) pushMidi (offset, (uint8_t) (0x80 | ((tr.outChannel - 1) & 15)), (uint8_t) e.note, 0);
            return;
        }
        if (e.lockOnly)
        {
            for (auto& v : voices)
                if (v.isAlive() && ! v.isFading() && v.track == t && (tr.machine != kKit || v.lane == e.lane))
                    v.legatoLocks (e.locks, e.nLocks);
            if (tr.machine == kMidiMachine) locksToCc (e, offset);
            return;
        }
        if (! trackAudible (t)) return;
        // MIDI out (MIDI machine, or doubling). A note played in with Thru on already went out as played (midiEvent).
        if ((tr.machine == kMidiMachine || tr.doubleMidi) && ! (e.source == 1 && proj->midi.thru && tr.machine != kMidiMachine))
        {
            const int ch = (tr.outChannel - 1) & 15;
            int note = e.note;
            if (tr.machine == kKit) note = proj->sounds[(size_t) t][(size_t) e.lane].i (LANE_NOTE);
            else if (e.source == 1) note = std::clamp ((int) std::lround (e.pitch), 0, 127);     // chord mode: each chord note
            if (tr.machine == kMidiMachine) locksToCc (e, offset);
            // Retrigger of a note still on: its note-off first.
            for (size_t i = 0; i < offQ.size(); ++i)
                if (offQ[i].d[0] == (0x80 | ch) && offQ[i].d[1] == note) { pushMidi (offset, offQ[i].d[0], offQ[i].d[1], 0); offQ.erase (offQ.begin() + (long) i); break; }
            pushMidi (offset, (uint8_t) (0x90 | ch), (uint8_t) note, (uint8_t) std::clamp ((int) std::lround (e.vel * 127.0f), 1, 127));
            if (e.gate > 0 && e.source != 1) offQ.push_back ({ absNow + e.gate, { (uint8_t) (0x80 | ch), (uint8_t) note } });
            if (e.source == 1) { auto& po = playedOut[(size_t) (t * 128 + (e.note & 127))]; if (po.n < 16) po.note[po.n++] = (uint8_t) note; }
        }
        startVoices (e, ppq, bpm, playing);
    }

    void locksToCc (const StartEv& e, int offset);

    // ── MIDI input ──
    int trackForChannel (int ch) const
    {
        const auto& M = proj->midi;
        if (M.harmonyChannel > 0 && ch == M.harmonyChannel) return -2;
        if (M.inMode == 0) return std::clamp (M.focus, 0, kTracks - 1);
        for (int t = 0; t < kTracks; ++t) if (proj->track[(size_t) t].inChannel == ch) return t;
        return -1;
    }
    int laneForNote (int t, int note) const
    {
        for (int l = 1; l <= 8; ++l) if (proj->sounds[(size_t) t][(size_t) l].i (LANE_NOTE) == note) return l;
        switch (note)
        {
            case 35: return 1; case 40: return 2; case 44: return 4; case 41: case 43: return 6;
            case 47: case 48: return 7; case 70: case 49: case 56: case 75: case 54: case 69: case 51: case 53: return 8;
            default: return note >= 36 && note < 44 ? note - 35 : -1;
        }
    }

    void midiEvent (const MidiIn& m, int offset, double ppq, double bpm, bool playing)
    {
        const uint8_t st = m.d[0] & 0xF0; const int ch = (m.d[0] & 0x0F) + 1;
        if (st < 0x80 || st >= 0xF0) return;
        const int t = trackForChannel (ch);
        if (t == -2)
        {
            if (st == 0x90 && m.d[2] > 0) harmonyHeld[m.d[1] & 127] = true;
            else if (st == 0x80 || (st == 0x90 && m.d[2] == 0)) harmonyHeld[m.d[1] & 127] = false;
            updateHarmony();
            return;
        }
        if (t < 0) return;
        auto& tr = proj->track[(size_t) t];
        auto& md = mod[(size_t) t];
        const int note = m.d[1] & 127;
        if (st == 0x90 && m.d[2] > 0)
        {
            const float vel = (float) m.d[2] / 127.0f;
            // THRU and MIDI machine: straight to the out channel.
            if (tr.machine == kMidiMachine || (proj->midi.thru && tr.doubleMidi)) pushMidi (offset, (uint8_t) (0x90 | ((tr.outChannel - 1) & 15)), (uint8_t) note, m.d[2]);
            if (tr.machine == kMidiMachine) { recordNote (t, 0, note, m.d[2], ppq, playing); return; }
            held[(size_t) (t * 128 + note)]++;
            if (tr.machine == kKit)
            {
                const int lane = laneForNote (t, note);
                if (lane < 1) return;
                StartEv s; s.track = t; s.lane = lane; s.note = note; s.pitch = 60.0f; s.vel = vel; s.source = 1; s.gate = -1;
                execStart (s, offset, ppq, bpm, playing);
                recordNote (t, lane - 1, note, m.d[2], ppq, playing);
                return;
            }
            const Sound& snd = proj->sounds[(size_t) t][0];
            if (snd.i (ARP_MODE) > 0) { arpAdd (t, note, ppq); recordNote (t, 0, note, m.d[2], ppq, playing); return; }
            int notes[16]; int nn = 0;
            if (tr.chordPlay)
            {
                const Pattern& pt = proj->bank.p[(size_t) std::clamp (cur.pattern, 0, kPatterns - 1)];
                const int rootPc = currentRoot (pt), sc = currentScale (pt);
                const ScaleDef& S = scaleOf (sc);
                const int q = quantizeToScale (note, rootPc, S);
                // Degree of q in the scale.
                int deg = 0; { const int rel = q - rootPc; const int o = floorDiv (rel, 12); const int pc = rel - 12 * o; for (int i = 0; i < S.n; ++i) if (S.s[i] == pc) deg = i + o * S.n; }
                Step tmp; tmp.chord = chDegree; tmp.chordSize = (uint8_t) std::clamp (tr.chordPlaySize, 2, 7); tmp.nNotes = 2; tmp.notes[0] = (int8_t) (deg - (4 - 1) * S.n + 0); tmp.notes[1] = 4;
                // Rebase: degreeNote(base at octave 4) — compute the degree relative to octave 4's root.
                const int base4 = 60 + rootPc; deg = 0; { const int rel = q - base4; const int o = floorDiv (rel, 12); const int pc = rel - 12 * o; for (int i = 0; i < S.n; ++i) if (S.s[i] == pc) deg = i + o * S.n; }
                tmp.notes[0] = (int8_t) deg;
                nn = buildChord (tmp, rootPc, sc, proj->chordMem, notes);
            }
            else notes[nn++] = note;
            for (int i = 0; i < nn; ++i)
            {
                StartEv s; s.track = t; s.lane = 0; s.note = note; s.pitch = (float) notes[i]; s.vel = vel; s.source = 1; s.gate = -1;
                execStart (s, offset, ppq, bpm, playing);
            }
            recordNote (t, 0, note, m.d[2], ppq, playing);
        }
        else if (st == 0x80 || st == 0x90)
        {
            if (tr.machine == kMidiMachine || (proj->midi.thru && tr.doubleMidi)) pushMidi (offset, (uint8_t) (0x80 | ((tr.outChannel - 1) & 15)), (uint8_t) note, 0);
            auto& h = held[(size_t) (t * 128 + note)]; if (h > 0) --h;
            if (proj->sounds[(size_t) t][0].i (ARP_MODE) > 0) arpRemove (t, note);
            if (! sustainOn[(size_t) t]) releaseNote (t, note, 1);
            if (tr.doubleMidi && tr.machine != kMidiMachine && ! proj->midi.thru)
            {
                // The note-offs of exactly the notes that went out for this key (a chord, a KIT lane's note).
                auto& po = playedOut[(size_t) (t * 128 + note)];
                if (po.n == 0) pushMidi (offset, (uint8_t) (0x80 | ((tr.outChannel - 1) & 15)), (uint8_t) note, 0);
                for (int i = 0; i < po.n; ++i) pushMidi (offset, (uint8_t) (0x80 | ((tr.outChannel - 1) & 15)), po.note[i], 0);
                po.n = 0;
            }
            recordOff (t, note, ppq, playing);
        }
        else if (st == 0xE0) { const int v = (m.d[1] | (m.d[2] << 7)) - 8192; md.bend = (float) v / 8192.0f; if (tr.machine == kMidiMachine) pushMidi (offset, (uint8_t) (0xE0 | ((tr.outChannel - 1) & 15)), m.d[1], m.d[2]); }
        else if (st == 0xD0) md.aftertouch = (float) m.d[1] / 127.0f;
        else if (st == 0xA0) { for (auto& v : voices) if (v.isAlive() && v.track == t && v.note == note) v.polyPressure = (float) m.d[2] / 127.0f; }
        else if (st == 0xB0)
        {
            const int cc = m.d[1]; const float val = (float) m.d[2] / 127.0f;
            if (cc == 1) md.wheel = val;
            else if (cc == 2) md.breath = val;
            else if (cc == 11) md.expression = val;
            else if (cc == 64)
            {
                const bool on = m.d[2] >= 64;
                if (! on && sustainOn[(size_t) t]) for (auto& v : voices) if (v.isAlive() && v.track == t && v.source == 1 && held[(size_t) (t * 128 + v.note)] == 0) v.release();
                sustainOn[(size_t) t] = on;
            }
            else if (cc == 120) for (auto& v : voices) { if (v.track == t) v.kill(); }
            else if (cc == 123) for (auto& v : voices) { if (v.track == t && v.source == 1) v.release(); }
        }
    }

    void updateHarmony()
    {
        int lo = -1; for (int i = 0; i < 128; ++i) if (harmonyHeld[(size_t) i]) { lo = i; break; }
        if (lo < 0) { harmonyRoot = -1; harmonyScale = -1; return; }
        harmonyRoot = lo % 12;
        bool m3 = false, M3 = false, b7 = false, M6 = false;
        for (int i = lo + 1; i < 128; ++i) if (harmonyHeld[(size_t) i]) { const int iv = (i - lo) % 12; m3 |= iv == 3; M3 |= iv == 4; b7 |= iv == 10; M6 |= iv == 9; }
        harmonyScale = M3 ? (b7 ? 5 : 0) : (m3 ? (M6 ? 2 : 1) : -1);
    }

    void recordNote (int t, int lane, int note, int vel, double ppq, bool playing)
    {
        if (! cur.rec || ! playing) return;
        const Segment* sg = seq.segmentAt (ppq);
        if (sg == nullptr) return;
        const auto& tp = proj->bank.p[(size_t) sg->pat].t[(size_t) t];
        const double sb = Sequencer::kStepBeats / speedMul (tp.speed);
        const double pos = (ppq - sg->start) / sb;
        int stepI = (int) std::floor (pos + 0.5);
        int micro = (int) std::lround ((pos - (double) stepI) * 48.0);
        const int L = Sequencer::laneLength (tp, lane);
        const int64_t g = std::llround ((sg->start - sg->entryStart) / sb) + stepI;
        stepI = (int) (((g % L) + L) % L);
        if (recOut.size() < recOut.capacity()) recOut.push_back ({ t, lane, stepI, note, vel, proj->midi.recQuantize ? 0 : std::clamp (micro, -24, 24), 12, sg->pat, false });
    }
    void recordOff (int t, int note, double ppq, bool playing)
    {
        if (! cur.rec || ! playing) return;
        RecEvent r; r.track = t; r.note = note; r.isNoteOff = true;
        const Segment* sg = seq.segmentAt (ppq);
        if (sg) { r.pattern = sg->pat; const auto& tp = proj->bank.p[(size_t) sg->pat].t[(size_t) t]; const double sb = Sequencer::kStepBeats / speedMul (tp.speed); r.step = (int) std::floor ((ppq - sg->start) / sb); }
        if (recOut.size() < recOut.capacity()) recOut.push_back (r);
    }

    // ── arpeggiator (MIDI-in notes of a TONE track) ──
    static double arpDiv (const Sound& snd) { return std::max (1.0 / 16.0, divisionBeats (std::max (1, snd.i (ARP_RATE)))); }
    void arpAdd (int t, int note, double ppq)
    {
        auto& a = arp[(size_t) t];
        for (int i = 0; i < a.n; ++i) if (a.notes[i] == note) return;
        if (a.n == 0) a.phase = (int64_t) std::ceil (ppq / arpDiv (proj->sounds[(size_t) t][0]) - 1e-9);
        if (a.n < 16) { a.notes[a.n++] = note; std::sort (a.notes, a.notes + a.n); }
    }
    void arpRemove (int t, int note)
    {
        auto& a = arp[(size_t) t];
        int k = 0; for (int i = 0; i < a.n; ++i) if (a.notes[i] != note) a.notes[k++] = a.notes[i];
        a.n = k;
        if (a.n == 0) a.lastTick = INT64_MIN;
    }
    // The arp ticks between samples [from, to) of the chunk (ppq0, abs0: its start). processChunk calls this after the
    // MIDI events of a position and up to the next one, so the notes held at each tick are the ones that sound.
    void arpTicks (int t, double ppq0, int64_t abs0, double rate, int from, int to, double bpm)
    {
        auto& a = arp[(size_t) t];
        const Sound& snd = proj->sounds[(size_t) t][0];
        const int mode = snd.i (ARP_MODE);
        if (mode == 0 || a.n == 0 || to <= from) return;
        const double div = arpDiv (snd);
        const int range = std::clamp (snd.i (ARP_RANGE), 1, 4);
        const int total = a.n * range;
        const int64_t k0 = (int64_t) std::ceil ((ppq0 + from * rate) / div - 1e-9), k1 = (int64_t) std::ceil ((ppq0 + to * rate) / div - 1e-9);
        for (int64_t k = k0; k < k1; ++k)
        {
            if (k == a.lastTick) continue;
            a.lastTick = k;
            const int64_t u = k - a.phase;      // steps since the arp started
            int idx;
            switch (mode)
            {
                case 2: idx = total - 1 - (int) (((u % total) + total) % total); break;
                case 3: { const int per = std::max (1, 2 * total - 2); const int w = (int) (((u % per) + per) % per); idx = w < total ? w : per - w; break; }
                case 4: idx = (int) (hashUni (77, (uint64_t) k, (uint64_t) t) * total); break;
                default: idx = (int) (((u % total) + total) % total); break;
            }
            idx = std::clamp (idx, 0, total - 1);
            const int note = a.notes[idx % a.n] + 12 * (idx / a.n);
            StartEv s; s.track = t; s.note = note; s.pitch = (float) note; s.vel = 0.8f; s.source = 2;
            s.gate = (int) (div * 0.8 * 60.0 / bpm * sr);
            s.at = abs0 + (int64_t) std::llround (((double) k * div - ppq0) / rate);
            pushStart (s);
        }
    }

    void processChunk (float* const* outs, int n, const HostTime& ht, const EngineParams& P, int chunkOffset);
};

// ───────────── implementation ─────────────
inline void Engine::locksToCc (const StartEv& e, int offset)
{
    // MIDI machine: step locks as CC on the track's channel (a lock on a sound parameter with a CC in the fixed map;
    // see MidiMap.h; here only the common ones, value normalised to 0..127).
    const auto& tr = proj->track[(size_t) e.track];
    const int ch = (tr.outChannel - 1) & 15;
    for (int i = 0; i < e.nLocks; ++i)
    {
        int cc = -1;
        switch (e.locks[i].id)
        {
            case CUTOFF: cc = 74; break; case RESO: cc = 71; break; case FENV: cc = 75; break; case DRIVE: cc = 77; break;
            case AGE: cc = 78; break; case SEND_DELAY: cc = 92; break; case SEND_REVERB: cc = 91; break; case PAN: cc = 10; break;
            case LEVEL: cc = 7; break; case LFO1_DEPTH: cc = 12; break; case LFO2_DEPTH: cc = 13; break;
            default: break;
        }
        if (cc < 0) continue;
        const int v = std::clamp ((int) std::lround (toNorm (e.locks[i].id, e.locks[i].v) * 127.0f), 0, 127);
        pushMidi (offset, (uint8_t) (0xB0 | ch), (uint8_t) cc, (uint8_t) v);
    }
}

inline void Engine::processChunk (float* const* outs, int n, const HostTime& ht, const EngineParams& P, int chunkOffset)
{
    cur = P;
    if (! smoothInit) { applyAutomation (true); smoothInit = true; }
    const bool playing = ht.playing;
    const double bpm = std::max (20.0, ht.bpm);
    const double rate = bpm / 60.0 / sr;                 // ppq per sample
    // A timeline for the LFO sync and the arpeggiator even with the transport stopped.
    double ppq0 = playing ? ht.ppq : freePpq;
    freePpq = playing ? ht.ppq + n * rate : freePpq + n * rate;

    // Transport: stop or jump → the sequencer's notes go.
    const bool jumped = playing && havePpq && std::fabs (ht.ppq - lastPpqEnd) > std::max (4.0 * rate, 1e-6) + jumpTol;
    if ((wasPlaying && ! playing) || jumped)
    {
        for (auto& v : voices) if (v.source == 0) v.release();
        for (auto& e : offQ) pushMidi (0, e.d[0], e.d[1], 0);
        offQ.clear();
        startQ.erase (std::remove_if (startQ.begin(), startQ.end(), [] (const StartEv& s) { return s.source == 0; }), startQ.end());
        for (auto& d : drone) d.n = 0;
        for (auto& f : fxLock) f.until = -1;
        seq.clearRolls();
        if (! playing) seq.reset();
    }
    if (! wasPlaying && playing) { seq.reset(); seq.clearRolls(); }
    wasPlaying = playing; havePpq = playing; lastPpqEnd = ht.ppq + n * rate;

    // Sequencer.
    SeqSettings st; st.pattern = std::clamp (P.pattern, 0, kPatterns - 1); st.song = P.song; st.fill = P.fill; st.seed = proj->seed; st.swing = P.swing;
    if (playing)
    {
        auto emit = [&] (const SeqEvent& e) { if (proj->track[(size_t) e.track].mute) return; queueStep (e, ht.ppq, rate, bpm); };
        seq.advance (ht.ppq, n, rate, st, emit);
        seq.flushRolls (ht.ppq, n, rate, emit);
    }

    // Clear scratch.
    for (auto& tb : tbuf) for (auto& b : tb) std::fill (b.begin(), b.begin() + n, 0.0f);
    for (auto& b : buf) std::fill (b.begin(), b.begin() + n, 0.0f);

    size_t mi = 0;
    int pos = 0, arpDone = 0;
    int64_t abs0 = absNow;
    while (pos < n)
    {
        absNow = abs0 + pos;
        // Rest state: nothing sounding, nothing queued and the output silent for a while: the absolute timeline (and
        // the stopped transport's free timeline) goes back to 0, so what comes next sounds the same whatever played
        // before (a project reopened, a second render, a fresh instance).
        if (absNow % kCtl == 0 && absNow != 0 && absNow - lastBusy >= 256 && startQ.empty() && offQ.empty() && activeSlots() == 0
            && std::all_of (fxLock.begin(), fxLock.end(), [] (const auto& f) { return f.until < 0; })
            && std::all_of (arp.begin(), arp.end(), [] (const Arp& a) { return a.n == 0; }))     // held arp notes count from their phase
        {
            const int64_t shift = absNow;
            abs0 -= shift; absNow = 0; lastBusy -= shift; loudD -= shift; loudR -= shift;
            for (auto& l : loudT) l -= shift;
            if (! playing) ppq0 = -pos * rate;      // the free timeline restarts here too
        }
        const double ppqHere = ppq0 + pos * rate;
        if (absNow % kCtl == 0)
        {
            compactVoices();
            applyAutomation (false);
            const int stb = kCtl;
            for (auto& v : voices) if (v.isAlive()) { const VoiceCtx c = ctxFor (v.track < 0 ? 0 : v.track, ppqHere, bpm, playing); v.control (c, stb); }
            // FX locks that ended.
            for (auto& f : fxLock) if (f.until >= 0 && f.until <= absNow) f.until = -1;
            for (int t = 0; t < kTracks; ++t) idleT[(size_t) t] = absNow - loudT[(size_t) t] > 4096;
            idleD = absNow - loudD > (int64_t) (4.3 * sr); idleR = absNow - loudR > (int64_t) (1.5 * sr);
        }
        // Events at this position: MIDI first, then queued starts and MIDI note-offs.
        while (mi < chunkMidi.size() && chunkMidi[mi].offset <= pos) { midiEvent (chunkMidi[mi], chunkOffset + pos, ppqHere, bpm, playing); ++mi; }
        // Arp ticks from here up to the next MIDI event, with this position's notes already in (DAW QA DV5: the whole
        // chunk used to be scheduled before its MIDI, so a chord pressed on a tick played a step late).
        if (const int horizon = mi < chunkMidi.size() ? std::max (pos + 1, chunkMidi[mi].offset) : n; horizon > arpDone)
        {
            for (int t = 0; t < kTracks; ++t) arpTicks (t, ppq0, abs0, rate, std::max (pos, arpDone), horizon, bpm);
            arpDone = horizon;
        }
        while (! startQ.empty() && startQ.front().at <= absNow) { const StartEv e = startQ.front(); startQ.erase (startQ.begin()); execStart (e, chunkOffset + pos, ppqHere, bpm, playing); }
        for (size_t i = 0; i < offQ.size();)
            if (offQ[i].at <= absNow) { pushMidi (chunkOffset + pos, offQ[i].d[0], offQ[i].d[1], 0); offQ.erase (offQ.begin() + (long) i); }
            else ++i;
        // Segment end: next control boundary, next event.
        int end = std::min (n, pos + samplesToBoundary (absNow));
        if (mi < chunkMidi.size()) end = std::min (end, std::max (pos + 1, chunkMidi[mi].offset));
        if (! startQ.empty()) end = std::min (end, (int) std::max ((int64_t) pos + 1, startQ.front().at - abs0));
        for (auto& o : offQ) end = std::min (end, (int) std::max ((int64_t) pos + 1, o.at - abs0));
        const int len = end - pos;
        // Voices, kQL at a time (slots kQL·q … kQL·q + kQL − 1 form group q).
        const int dk = duckSource();
        std::array<bool, kTracks> hasVoice {};
        for (int q = 0; q < kSlots / kQL; ++q)
        {
            Voice* vq[kQL]; VoiceOut oq[kQL]; bool any = false;
            for (int l = 0; l < kQL; ++l)
            {
                Voice& v = voices[(size_t) (kQL * q + l)];
                vq[l] = nullptr;
                if (! v.isAlive() || v.track < 0) continue;
                const int t = v.track;
                VoiceOut& o = oq[l];
                o.fmL = tbuf[(size_t) t][tFmL].data(); o.fmR = tbuf[(size_t) t][tFmR].data(); o.sub = tbuf[(size_t) t][tSub].data();
                o.dL = tbuf[(size_t) t][tDL].data(); o.dR = tbuf[(size_t) t][tDR].data(); o.rL = tbuf[(size_t) t][tRL].data(); o.rR = tbuf[(size_t) t][tRR].data();
                o.key = (t == dk && v.lane == 1) ? buf[gKey].data() : nullptr;
                vq[l] = &v; any = true; hasVoice[(size_t) t] = true;
            }
            if (! any) continue;
            // Lanes 4-7 empty (the compaction keeps groups packed from lane 0): one register wide is enough.
            bool upper = false; for (int l = 4; l < kQL; ++l) upper |= vq[l] != nullptr;
            if (upper) Voice::renderGroup<V8, 8> (vq, oq, len, pos, absNow);
            else Voice::renderGroup<V4, 4> (vq, oq, len, pos, absNow);
        }
        // Duck gains of this segment, from the KIT kick lane just rendered.
        {
            const float* K = buf[gKey].data() + pos; float* D = buf[gDuck].data() + pos;
            for (int i = 0; i < len; ++i) D[i] = ducker.process (K[i], cur.duck, proj->fx.duckRelease);
        }
        // Track buses.
        for (int t = 0; t < kTracks; ++t)
        {
            auto& B = tbuf[(size_t) t];
            float* L = B[tFmL].data() + pos; float* R = B[tFmR].data() + pos; float* S = B[tSub].data() + pos;
            const Sound& s0 = proj->sounds[(size_t) t][0];
            const auto& bt = busTrig[(size_t) t];
            auto busVal = [&] (int p) {
                for (int i = 0; i < bt.n; ++i) if (bt.locks[i].id == p) return bt.locks[i].v;
                return bt.patch >= 0 ? proj->patches[(size_t) bt.patch][p] : s0[p];
            };
            const bool kit = proj->track[(size_t) t].machine == kKit;
            const bool busOn = hasVoice[(size_t) t] || ! idleT[(size_t) t];
            if (kit && busOn)
            {
                // Kit bus: low-pass (open at 20 kHz) and drive.
                const float cut = s0[CUTOFF], dr = s0[DRIVE];
                if (cut < 19500.0f) { kitLp[(size_t) t][0].set (cut, 0.6f + 6.0f * s0[RESO] * s0[RESO], (float) sr); kitLp[(size_t) t][1].set (cut, 0.6f + 6.0f * s0[RESO] * s0[RESO], (float) sr);
                                       for (int i = 0; i < len; ++i) { L[i] = kitLp[(size_t) t][0].lowpass (L[i]); R[i] = kitLp[(size_t) t][1].lowpass (R[i]); } }
                if (dr > 0.001f)
                {
                    const float g = dbToGain (dr * 30.0f), comp = 1.0f / (0.6f + 0.4f * std::sqrt (g));
                    for (int i = 0; i < len; ++i)
                        for (int c = 0; c < 2; ++c)
                        {
                            float* X = c == 0 ? L : R;
                            const float y = fastTanh (g * X[i] + 0.1f * dr) - fastTanh (0.1f * dr);
                            const float dy = y - kitDcX[t][c] + 0.9995f * kitDcY[t][c]; kitDcX[t][c] = y; kitDcY[t][c] = dy;
                            X[i] = dy * comp;
                        }
                }
            }
            InsertParams ip; ip.type = (int) (busVal (INS_TYPE) + 0.5f); ip.amt = busVal (INS_AMT); ip.p1 = busVal (INS_P1); ip.p2 = busVal (INS_P2); ip.mix = busVal (INS_MIX);
            const Pattern& pt = proj->bank.p[(size_t) std::clamp (P.pattern, 0, kPatterns - 1)];
            ip.rootHz = mtof (36.0f + (float) currentRoot (pt));
            if (busOn)
            {
                ins[(size_t) t].process (L, R, len, ip, absNow);
                tape[(size_t) t].process (L, R, len, busVal (AGE), absNow, (uint32_t) (t * 977 + 5));
                const int ll = hasVoice[(size_t) t] ? len - 1 : lastLoud (L, R, len);
                if (ll >= 0) loudT[(size_t) t] = absNow + ll + 1;
            }
            const float fader = trackAudible (t) ? dbToGain (sm[(size_t) t].levelDb) : 0.0f;
            const float sd = kit ? s0[SEND_DELAY] : busVal (SEND_DELAY), sr_ = kit ? s0[SEND_REVERB] : busVal (SEND_REVERB);
            const bool ducked = proj->track[(size_t) t].machine == kTone;
            const int dlat = Voice::kLatency + tape[(size_t) t].latency();
            const bool lm = s0[LOWMONO] > 0.5f;
            float* DL = buf[gDL].data() + pos; float* DR = buf[gDR].data() + pos; float* RL = buf[gRL].data() + pos; float* RR = buf[gRR].data() + pos;
            float* const* mainOut = outs;
            float* oL = outs[2 + 2 * t] && proj->track[(size_t) t].outBus == 1 ? outs[2 + 2 * t] + pos : nullptr;
            float* oR = outs[3 + 2 * t] && proj->track[(size_t) t].outBus == 1 ? outs[3 + 2 * t] + pos : nullptr;
            float* mL = buf[gML].data() + pos; float* mR = buf[gMR].data() + pos;
            const float* duckG = buf[gDuck].data() + pos;
            (void) mainOut;
            for (int i = 0; i < len; ++i)
            {
                const float g = fader * (ducked ? duckG[i] : 1.0f);
                float l = L[i], r = R[i];
                // Sends after the insert and the tape (TONE); the KIT lanes also sent per voice (pre-insert).
                // Send levels glide (≈ 8 ms): a per-step throw (a send lock) does not click.
                auto& ss = sendSm[(size_t) t];
                if (ss[0] < 0.0f) { ss[0] = sd; ss[1] = sr_; }
                ss[0] += (sd - ss[0]) * sendK; ss[1] += (sr_ - ss[1]) * sendK;
                DL[i] += (l * ss[0] + B[tDL][(size_t) (pos + i)]) * g; DR[i] += (r * ss[0] + B[tDR][(size_t) (pos + i)]) * g;
                RL[i] += (l * ss[1] + B[tRL][(size_t) (pos + i)]) * g; RR[i] += (r * ss[1] + B[tRR][(size_t) (pos + i)]) * g;
                // Sub: clean, aligned with the decimator + tape delay.
                subDelay[(size_t) t].push (S[i]);
                const float sub = subDelay[(size_t) t].at (dlat - Voice::kLatency + 1);
                l += sub; r += sub;
                if (lm) lowMono[(size_t) t].process (l, r);
                l *= g; r *= g;
                if (oL)
                {
                    // Own output: delayed like the master (its tape base delay) so it stays aligned with the main mix.
                    ownDelay[(size_t) t][0].push (l); ownDelay[(size_t) t][1].push (r);
                    oL[i] = ownDelay[(size_t) t][0].at (master.latency() + 1); oR[i] = ownDelay[(size_t) t][1].at (master.latency() + 1);
                }
                else { mL[i] += l; mR[i] += r; }
            }
        }
        // Sends.
        {
            float* DL = buf[gDL].data() + pos; float* DR = buf[gDR].data() + pos; float* RL = buf[gRL].data() + pos; float* RR = buf[gRR].data() + pos;
            auto fxv = [&] (int id, float def) { const auto& f = fxLock[(size_t) (id - kGlobalLockBase)]; return f.until >= 0 ? f.v : def; };
            DelayParams dp; dp.mode = proj->fx.delayMode; dp.feedback = fxv (gDelayFeedback, P.delayFb); dp.tone = proj->fx.delayTone; dp.wow = proj->fx.delayWow;
            dp.ret = proj->fx.delayReturn; dp.freeze = fxv (gDelayFreeze, proj->fx.delayFreeze ? 1.0f : 0.0f) > 0.5f;
            const double beats = fxLock[(size_t) (gDelayTime - kGlobalLockBase)].until >= 0 ? delayDivBeats ((int) fxLock[(size_t) (gDelayTime - kGlobalLockBase)].v) : delayDivBeats (P.delayDiv);
            dp.timeS = (float) (beats * 60.0 / bpm);
            {
                const int li = lastLoud (DL, DR, len);
                if (li >= 0 || ! idleD)
                {
                    if (li >= 0) loudD = absNow + len;
                    delay.process (DL, DR, len, dp, absNow);
                    const int lo = lastLoud (DL, DR, len);
                    if (lo >= 0) loudD = std::max (loudD, absNow + lo + 1);
                }
            }
            const float toRev = proj->fx.delayToReverb;
            for (int i = 0; i < len; ++i) { RL[i] += DL[i] * toRev; RR[i] += DR[i] * toRev; }
            ReverbParams rp; rp.mode = proj->fx.reverbMode; rp.size = P.revSize; rp.decayS = fxv (gReverbDecay, P.revDecay); rp.damp = proj->fx.reverbDamp;
            rp.predelayS = proj->fx.reverbPredelay; rp.mod = proj->fx.reverbMod; rp.shimmer = fxv (gReverbShimmer, proj->fx.reverbShimmer); rp.ret = proj->fx.reverbReturn;
            rp.freeze = fxv (gReverbFreeze, proj->fx.reverbFreeze ? 1.0f : 0.0f) > 0.5f;
            rp.freezeArmed = proj->fx.reverbFreeze || freezeLocks;
            {
                const int li = lastLoud (RL, RR, len);
                if (li >= 0 || ! idleR)
                {
                    if (li >= 0) loudR = absNow + len;
                    reverb.process (RL, RR, len, rp);
                    const int lo = lastLoud (RL, RR, len);
                    if (lo >= 0) loudR = std::max (loudR, absNow + lo + 1);
                }
            }
            float* mL = buf[gML].data() + pos; float* mR = buf[gMR].data() + pos;
            for (int i = 0; i < len; ++i) { mL[i] += DL[i] + RL[i]; mR[i] += DR[i] + RR[i]; }
            MasterParams mp; mp.tape = P.tape; mp.noise = fxv (gTapeNoise, proj->fx.tapeNoise); mp.glue = P.glue; mp.limit = P.limit; mp.volumeDb = P.masterDb;
            master.process (mL, mR, len, mp, absNow);
            for (int i = len - 1; i >= 0; --i) if (std::fabs (mL[i]) > 1.0e-6f || std::fabs (mR[i]) > 1.0e-6f) { lastBusy = absNow + i + 1; break; }
        }
        pos = end;
    }
    absNow = abs0 + n;
    if (! playing) freePpq = ppq0 + n * rate;
    // Main out.
    for (int i = 0; i < n; ++i) { outs[0][i] += buf[gML][(size_t) i]; outs[1][i] += buf[gMR][(size_t) i]; }
    // Track outputs that go to their own bus also pass the master delay so they stay aligned with the main mix.
    view.playing = playing; view.ppq = ppq0; view.grDb = master.gainReductionDb(); view.activeVoices = activeVoices();
    if (playing) if (const Segment* sg = seq.segmentAt (ht.ppq))
    {
        view.pattern = sg->pat;
        for (int t = 0; t < kTracks; ++t) for (int l = 0; l < kLanes; ++l) view.step[(size_t) (t * kLanes + l)] = seq.stepOf (*sg, t, l, ht.ppq);
    }
}

} // namespace dh::dv
