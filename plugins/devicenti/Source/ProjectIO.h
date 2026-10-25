#pragma once
// DeVicenti: the project (sounds, patterns, chain, chord memory, effect modes, MIDI settings) to and from JSON.
// The same format is the factory preset file (tools/presets.py writes it), the user preset file and the part of the
// plugin state that is not a host parameter. Only values that differ from the defaults are written; numbers are
// rounded to 6 significant digits, so saving, loading and saving again gives the same bytes. Parameter keys are the
// stable keys of SoundParams.h.

#include <JuceHeader.h>
#include "dsp/Engine.h"

namespace dh::dv::pio {

inline double r6 (double x) { if (x == 0.0 || ! std::isfinite (x)) return 0.0; char b[32]; std::snprintf (b, sizeof (b), "%.6g", x); return std::atof (b); }

inline const char* machineName (int m) { return m == kKit ? "KIT" : (m == kMidiMachine ? "MIDI" : "TONE"); }
inline int machineFrom (const juce::String& s) { return s == "KIT" ? kKit : (s == "MIDI" ? kMidiMachine : kTone); }
inline const char* condKey (int c) { static const char* k[kNumConds] = { "none", "fill", "!fill", "pre", "!pre", "nei", "!nei", "1st", "!1st", "last", "!last", "ab", "bar" }; return k[std::clamp (c, 0, (int) kNumConds - 1)]; }
inline int condFrom (const juce::String& s) { for (int c = 0; c < kNumConds; ++c) if (s.equalsIgnoreCase (condKey (c))) return c; return 0; }

// Defaults that depend on where a sound sits: a KIT lane plays its GM-like note; a kit bus starts open, unity groups.
inline Sound defaultSound (int machine, int lane)
{
    Sound s;
    if (machine == kKit)
    {
        if (lane == 0) { s[CUTOFF] = 20000.0f; for (int o = 0; o < 4; ++o) s[opParam (o, oLevel)] = 0.75f; s[FTYPE] = kFilterOff; }
        else { s[LANE_NOTE] = (float) defaultLaneNote (lane); s[FTYPE] = kFilterOff; s[CUTOFF] = 20000.0f; s[AMP_SUS] = 1.0f; s[AMP_REL] = 0.1f; }
    }
    return s;
}

inline juce::var soundToVar (const Sound& s, const Sound& def)
{
    auto* o = new juce::DynamicObject();
    const auto& info = paramInfo();
    for (int p = 0; p < kNumSoundParams; ++p)
        if (std::fabs (s[p] - def[p]) > 1e-7f * std::max (1.0f, std::fabs (def[p]))) o->setProperty (info[(size_t) p].key, r6 (s[p]));
    return juce::var (o);
}
inline void soundFromVar (const juce::var& v, Sound& s)
{
    if (auto* o = v.getDynamicObject())
        for (auto& kv : o->getProperties())
        {
            const int p = paramIndex (kv.name.toString().toRawUTF8());
            if (p >= 0) s[p] = clampParam (p, (float) (double) kv.value);
        }
}

inline juce::var stepToVar (const Step& s, int lane, int idx, const Pattern& pt)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("lane", lane); o->setProperty ("i", idx);
    if (s.trig != 1) o->setProperty ("trig", (int) s.trig);
    if (s.nNotes > 0) { juce::Array<juce::var> n; for (int k = 0; k < std::min ((int) s.nNotes, kMaxNotes); ++k) n.add ((int) s.notes[k]); o->setProperty ("notes", n); }
    if (s.vel != 100) o->setProperty ("vel", (int) s.vel);
    if (s.len != 12) o->setProperty ("len", s.len == Step::kTie ? -1 : (int) s.len);
    if (s.micro != 0) o->setProperty ("micro", (int) s.micro);
    if (s.retrig) o->setProperty ("retrig", juce::Array<juce::var> { (int) s.retrig, (int) s.retrigCount, (int) s.retrigRamp });
    if (s.cond != cNone) { o->setProperty ("cond", condKey (s.cond)); if (s.cond == cAB || s.cond == cBar) { o->setProperty ("a", (int) s.condA); o->setProperty ("b", (int) s.condB); } }
    if (s.prob != 100) o->setProperty ("prob", (int) s.prob);
    if (s.chord != chOff)
    {
        auto* c = new juce::DynamicObject();
        c->setProperty ("kind", s.chord == chMemory ? "memory" : "degree");
        c->setProperty ("type", (int) s.chordType); c->setProperty ("size", (int) s.chordSize); c->setProperty ("voicing", (int) s.voicing);
        if (s.inversion) c->setProperty ("inversion", (int) s.inversion);
        if (s.chordFlags) c->setProperty ("flags", (int) s.chordFlags);
        o->setProperty ("chord", juce::var (c));
    }
    if (s.patch >= 0) o->setProperty ("patch", (int) s.patch);
    if (s.flags & sfSlide) o->setProperty ("slide", 1);
    if (s.lockCount > 0)
    {
        auto* L = new juce::DynamicObject();
        for (int k = 0; k < s.lockCount; ++k)
        {
            const auto& pl = pt.pool[(size_t) std::min ((int) s.lockStart + k, kPoolLocks - 1)];
            const char* key = pl.id >= kGlobalLockBase ? globalLockKey (pl.id) : (pl.id >= 0 && pl.id < kNumSoundParams ? paramInfo()[(size_t) pl.id].key : "");
            if (*key) L->setProperty (key, r6 (pl.v));
        }
        o->setProperty ("locks", juce::var (L));
    }
    return juce::var (o);
}

inline void stepFromVar (const juce::var& v, Step& s, Pattern& pt)
{
    s = Step(); s.trig = (uint8_t) juce::jlimit (0, 2, (int) v.getProperty ("trig", 1));
    if (auto* n = v.getProperty ("notes", {}).getArray()) { s.nNotes = (uint8_t) std::min (n->size(), kMaxNotes); for (int k = 0; k < s.nNotes; ++k) s.notes[k] = (int8_t) juce::jlimit (-64, 127, (int) (*n)[k]); }
    s.vel = (uint8_t) juce::jlimit (1, 127, (int) v.getProperty ("vel", 100));
    { const int l = (int) v.getProperty ("len", 12); s.len = l < 0 ? Step::kTie : (uint16_t) juce::jlimit (1, 64 * 12, l); }
    s.micro = (int8_t) juce::jlimit (-24, 24, (int) v.getProperty ("micro", 0));
    if (auto* r = v.getProperty ("retrig", {}).getArray(); r != nullptr && r->size() >= 2)
    { s.retrig = (uint8_t) juce::jlimit (0, 5, (int) (*r)[0]); s.retrigCount = (uint8_t) juce::jlimit (2, 16, (int) (*r)[1]); s.retrigRamp = (int8_t) juce::jlimit (-127, 127, r->size() > 2 ? (int) (*r)[2] : 0); }
    if (v.hasProperty ("cond")) { s.cond = (uint8_t) condFrom (v.getProperty ("cond", "none").toString()); s.condA = (uint8_t) juce::jlimit (1, 64, (int) v.getProperty ("a", 1)); s.condB = (uint8_t) juce::jlimit (1, 64, (int) v.getProperty ("b", 2)); }
    s.prob = (uint8_t) juce::jlimit (0, 100, (int) v.getProperty ("prob", 100));
    if (auto* c = v.getProperty ("chord", {}).getDynamicObject())
    {
        s.chord = c->getProperty ("kind").toString() == "memory" ? chMemory : chDegree;
        s.chordType = (uint8_t) juce::jlimit (0, (int) kNumChordTypes - 1, (int) c->getProperty ("type"));
        s.chordSize = (uint8_t) juce::jlimit (2, 7, c->hasProperty ("size") ? (int) c->getProperty ("size") : 3);
        s.voicing = (uint8_t) juce::jlimit (0, (int) kNumVoicings - 1, (int) c->getProperty ("voicing"));
        s.inversion = (uint8_t) juce::jlimit (0, 6, (int) c->getProperty ("inversion"));
        s.chordFlags = (uint8_t) juce::jlimit (0, 255, (int) c->getProperty ("flags"));
    }
    s.patch = (int8_t) juce::jlimit (-1, kPatches - 1, (int) v.getProperty ("patch", -1));
    if ((int) v.getProperty ("slide", 0)) s.flags |= sfSlide;
    if (auto* L = v.getProperty ("locks", {}).getDynamicObject())
    {
        s.lockStart = pt.nLocks; s.lockCount = 0;
        for (auto& kv : L->getProperties())
        {
            const int id = lockIndex (kv.name.toString().toRawUTF8());
            if (id < 0 || pt.nLocks >= kPoolLocks || s.lockCount >= 255) continue;
            float val = (float) (double) kv.value;
            if (id < kNumSoundParams) val = clampParam (id, val);
            pt.pool[(size_t) pt.nLocks++] = { (int16_t) id, val };
            ++s.lockCount;
        }
    }
}

inline juce::var patternToVar (const Pattern& p, int index)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("index", index);
    o->setProperty ("length", (int) p.length); o->setProperty ("root", (int) p.root); o->setProperty ("scale", (int) p.scale);
    juce::Array<juce::var> tracks;
    for (int t = 0; t < kTracks; ++t)
    {
        const auto& tp = p.t[(size_t) t];
        auto* to = new juce::DynamicObject();
        to->setProperty ("length", (int) tp.length);
        if (tp.speed != 4) to->setProperty ("speed", (int) tp.speed);
        if (tp.swing != 100) to->setProperty ("swing", (int) tp.swing);
        bool anyLane = false; for (auto l : tp.laneLen) anyLane |= l != 0;
        if (anyLane) { juce::Array<juce::var> ll; for (auto l : tp.laneLen) ll.add ((int) l); to->setProperty ("lane_len", ll); }
        juce::Array<juce::var> steps;
        for (int l = 0; l < kLanes; ++l) for (int i = 0; i < kMaxSteps; ++i) if (tp.lane[(size_t) l][(size_t) i].on()) steps.add (stepToVar (tp.lane[(size_t) l][(size_t) i], l, i, p));
        to->setProperty ("steps", steps);
        tracks.add (juce::var (to));
    }
    o->setProperty ("tracks", tracks);
    return juce::var (o);
}

inline void patternFromVar (const juce::var& v, Pattern& p)
{
    p = Pattern();
    p.length = (uint8_t) juce::jlimit (1, kMaxSteps, (int) v.getProperty ("length", 16));
    p.root = (uint8_t) juce::jlimit (0, 11, (int) v.getProperty ("root", 9));
    p.scale = (uint8_t) juce::jlimit (0, (int) scales().size() - 1, (int) v.getProperty ("scale", 1));
    if (auto* tr = v.getProperty ("tracks", {}).getArray())
        for (int t = 0; t < std::min (tr->size(), kTracks); ++t)
        {
            const auto& tv = (*tr)[t];
            auto& tp = p.t[(size_t) t];
            tp.length = (uint8_t) juce::jlimit (1, kMaxSteps, (int) tv.getProperty ("length", p.length));
            tp.speed = (uint8_t) juce::jlimit (0, 6, (int) tv.getProperty ("speed", 4));
            tp.swing = (uint8_t) juce::jlimit (0, 100, (int) tv.getProperty ("swing", 100));
            if (auto* ll = tv.getProperty ("lane_len", {}).getArray()) for (int l = 0; l < std::min (ll->size(), kLanes); ++l) tp.laneLen[(size_t) l] = (uint8_t) juce::jlimit (0, kMaxSteps, (int) (*ll)[l]);
            if (auto* st = tv.getProperty ("steps", {}).getArray())
                for (const auto& sv : *st)
                {
                    const int l = juce::jlimit (0, kLanes - 1, (int) sv.getProperty ("lane", 0)), i = juce::jlimit (0, kMaxSteps - 1, (int) sv.getProperty ("i", 0));
                    stepFromVar (sv, tp.lane[(size_t) l][(size_t) i], p);
                }
        }
}

inline juce::var fxToVar (const FxSettings& f)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("delay_mode", f.delayMode); o->setProperty ("delay_tone", r6 (f.delayTone)); o->setProperty ("delay_wow", r6 (f.delayWow));
    o->setProperty ("delay_return", r6 (f.delayReturn)); o->setProperty ("delay_to_reverb", r6 (f.delayToReverb));
    o->setProperty ("reverb_mode", f.reverbMode); o->setProperty ("reverb_damp", r6 (f.reverbDamp)); o->setProperty ("reverb_predelay", r6 (f.reverbPredelay));
    o->setProperty ("reverb_mod", r6 (f.reverbMod)); o->setProperty ("reverb_shimmer", r6 (f.reverbShimmer)); o->setProperty ("reverb_return", r6 (f.reverbReturn));
    o->setProperty ("delay_freeze", f.delayFreeze ? 1 : 0); o->setProperty ("reverb_freeze", f.reverbFreeze ? 1 : 0);
    o->setProperty ("tape_noise", r6 (f.tapeNoise)); o->setProperty ("duck_release", r6 (f.duckRelease)); o->setProperty ("duck_track", f.duckTrack);
    return juce::var (o);
}
inline void fxFromVar (const juce::var& v, FxSettings& f)
{
    f = FxSettings();
    auto F = [&] (const char* k, float d) { return (float) (double) v.getProperty (k, d); };
    f.delayMode = juce::jlimit (0, 2, (int) v.getProperty ("delay_mode", f.delayMode)); f.delayTone = F ("delay_tone", f.delayTone); f.delayWow = F ("delay_wow", f.delayWow);
    f.delayReturn = F ("delay_return", f.delayReturn); f.delayToReverb = F ("delay_to_reverb", f.delayToReverb);
    f.reverbMode = juce::jlimit (0, 3, (int) v.getProperty ("reverb_mode", f.reverbMode)); f.reverbDamp = F ("reverb_damp", f.reverbDamp); f.reverbPredelay = F ("reverb_predelay", f.reverbPredelay);
    f.reverbMod = F ("reverb_mod", f.reverbMod); f.reverbShimmer = F ("reverb_shimmer", f.reverbShimmer); f.reverbReturn = F ("reverb_return", f.reverbReturn);
    f.delayFreeze = (int) v.getProperty ("delay_freeze", 0) != 0; f.reverbFreeze = (int) v.getProperty ("reverb_freeze", 0) != 0;
    f.tapeNoise = F ("tape_noise", f.tapeNoise); f.duckRelease = F ("duck_release", f.duckRelease); f.duckTrack = juce::jlimit (-1, kTracks - 1, (int) v.getProperty ("duck_track", -1));
}

inline juce::var midiToVar (const MidiSettings& m)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("in_mode", m.inMode); o->setProperty ("focus", m.focus); o->setProperty ("cc_mode", m.ccMode); o->setProperty ("cc_feedback", m.ccFeedback ? 1 : 0);
    o->setProperty ("pc_mode", m.pcMode); o->setProperty ("harmony_channel", m.harmonyChannel); o->setProperty ("clock_in", m.clockIn ? 1 : 0); o->setProperty ("clock_out", m.clockOut ? 1 : 0);
    o->setProperty ("thru", m.thru ? 1 : 0); o->setProperty ("rec_quantize", m.recQuantize);
    return juce::var (o);
}
inline void midiFromVar (const juce::var& v, MidiSettings& m)
{
    m = MidiSettings();
    m.inMode = juce::jlimit (0, 1, (int) v.getProperty ("in_mode", m.inMode)); m.focus = juce::jlimit (0, kTracks - 1, (int) v.getProperty ("focus", m.focus));
    m.ccMode = juce::jlimit (0, 2, (int) v.getProperty ("cc_mode", m.ccMode)); m.ccFeedback = (int) v.getProperty ("cc_feedback", 0) != 0;
    m.pcMode = juce::jlimit (0, 1, (int) v.getProperty ("pc_mode", m.pcMode)); m.harmonyChannel = juce::jlimit (0, 16, (int) v.getProperty ("harmony_channel", 0));
    m.clockIn = (int) v.getProperty ("clock_in", 0) != 0; m.clockOut = (int) v.getProperty ("clock_out", 1) != 0; m.thru = (int) v.getProperty ("thru", 0) != 0;
    m.recQuantize = juce::jlimit (0, 1, (int) v.getProperty ("rec_quantize", 1));
}

// The whole project. `withMidi`: the MIDI settings travel in the session state, not in presets.
inline juce::var projectToVar (const Project& pr, bool withMidi)
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> tracks;
    for (int t = 0; t < kTracks; ++t)
    {
        const auto& ts = pr.track[(size_t) t];
        auto* to = new juce::DynamicObject();
        to->setProperty ("machine", machineName (ts.machine));
        if (ts.mute) to->setProperty ("mute", 1);
        if (ts.solo) to->setProperty ("solo", 1);
        to->setProperty ("in_channel", ts.inChannel); to->setProperty ("out_channel", ts.outChannel);
        if (ts.doubleMidi) to->setProperty ("double", 1);
        if (ts.outBus) to->setProperty ("out_bus", ts.outBus);
        if (ts.chordPlay) to->setProperty ("chord_play", 1);
        if (ts.chordPlaySize != 3) to->setProperty ("chord_play_size", ts.chordPlaySize);
        auto* so = new juce::DynamicObject();
        const int nS = ts.machine == kKit ? kSoundsPerTrack : 1;
        for (int l = 0; l < nS; ++l) so->setProperty (juce::String (l), soundToVar (pr.sounds[(size_t) t][(size_t) l], defaultSound (ts.machine, l)));
        to->setProperty ("sounds", juce::var (so));
        tracks.add (juce::var (to));
    }
    o->setProperty ("tracks", tracks);
    juce::Array<juce::var> patches;
    int lastPatch = -1; const Sound def;
    for (int i = 0; i < kPatches; ++i) { bool same = true; for (int p = 0; p < kNumSoundParams && same; ++p) same = pr.patches[(size_t) i][p] == def[p]; if (! same) lastPatch = i; }
    for (int i = 0; i <= lastPatch; ++i) patches.add (soundToVar (pr.patches[(size_t) i], def));
    if (! patches.isEmpty()) o->setProperty ("patches", patches);
    juce::Array<juce::var> mem;
    for (const auto& m : pr.chordMem) { juce::Array<juce::var> a; for (int k = 0; k < m.n; ++k) a.add ((int) m.iv[k]); mem.add (a); }
    o->setProperty ("chord_mem", mem);
    juce::Array<juce::var> pats;
    for (int i = 0; i < kPatterns; ++i)
    {
        const auto& p = pr.bank.p[(size_t) i];
        if (p.isEmpty() && p.length == 16 && p.root == 9 && p.scale == 1) continue;
        pats.add (patternToVar (p, i));
    }
    o->setProperty ("patterns", pats);
    juce::Array<juce::var> chain;
    for (int i = 0; i < pr.bank.chainLen; ++i) { const auto& c = pr.bank.chain[(size_t) i]; chain.add (juce::Array<juce::var> { (int) c.pattern, (int) c.repeats, c.fill ? 1 : 0, (int) c.muteMask }); }
    o->setProperty ("chain", chain);
    o->setProperty ("fx", fxToVar (pr.fx));
    if (withMidi) o->setProperty ("midi", midiToVar (pr.midi));
    o->setProperty ("seed", (int) pr.seed);
    return juce::var (o);
}

inline void projectFromVar (const juce::var& v, Project& pr, bool withMidi)
{
    const MidiSettings keepMidi = pr.midi;
    // Reset in place (the project is ~1.2 MB: no temporary on the stack).
    for (auto& t : pr.track) t = TrackSettings();
    for (int t = 0; t < kTracks; ++t) pr.track[(size_t) t].inChannel = pr.track[(size_t) t].outChannel = t + 1;
    for (auto& p : pr.patches) p = Sound();
    for (auto& m : pr.chordMem) m = ChordMem();
    for (auto& p : pr.bank.p) p = Pattern();
    pr.bank.chainLen = 0; pr.fx = FxSettings(); pr.seed = 1;
    if (auto* tr = v.getProperty ("tracks", {}).getArray())
        for (int t = 0; t < std::min (tr->size(), kTracks); ++t)
        {
            const auto& tv = (*tr)[t];
            auto& ts = pr.track[(size_t) t];
            ts.machine = machineFrom (tv.getProperty ("machine", "TONE").toString());
            ts.mute = (int) tv.getProperty ("mute", 0) != 0; ts.solo = (int) tv.getProperty ("solo", 0) != 0;
            ts.inChannel = juce::jlimit (1, 16, (int) tv.getProperty ("in_channel", t + 1)); ts.outChannel = juce::jlimit (1, 16, (int) tv.getProperty ("out_channel", t + 1));
            ts.doubleMidi = (int) tv.getProperty ("double", 0) != 0; ts.outBus = juce::jlimit (0, 1, (int) tv.getProperty ("out_bus", 0));
            ts.chordPlay = (int) tv.getProperty ("chord_play", 0) != 0; ts.chordPlaySize = juce::jlimit (2, 7, (int) tv.getProperty ("chord_play_size", 3));
            const auto sv = tv.getProperty ("sounds", {});
            for (int l = 0; l < kSoundsPerTrack; ++l)
            {
                Sound s = defaultSound (ts.machine, l);
                soundFromVar (sv.getProperty (juce::String (l), {}), s);
                pr.sounds[(size_t) t][(size_t) l] = s;
            }
        }
    else
        for (int t = 0; t < kTracks; ++t) for (int l = 0; l < kSoundsPerTrack; ++l) pr.sounds[(size_t) t][(size_t) l] = defaultSound (kTone, l);
    if (auto* pa = v.getProperty ("patches", {}).getArray()) for (int i = 0; i < std::min (pa->size(), kPatches); ++i) soundFromVar ((*pa)[i], pr.patches[(size_t) i]);
    if (auto* mem = v.getProperty ("chord_mem", {}).getArray())
        for (int i = 0; i < std::min (mem->size(), kChordMems); ++i)
            if (auto* a = (*mem)[i].getArray()) { auto& m = pr.chordMem[(size_t) i]; m.n = (uint8_t) std::min (a->size(), kMaxNotes); for (int k = 0; k < m.n; ++k) m.iv[k] = (int8_t) juce::jlimit (-48, 48, (int) (*a)[k]); }
    if (auto* pa = v.getProperty ("patterns", {}).getArray())
        for (const auto& pv : *pa) { const int i = juce::jlimit (0, kPatterns - 1, (int) pv.getProperty ("index", 0)); patternFromVar (pv, pr.bank.p[(size_t) i]); }
    if (auto* ch = v.getProperty ("chain", {}).getArray())
        for (const auto& e : *ch)
            if (auto* a = e.getArray(); a != nullptr && a->size() >= 2 && pr.bank.chainLen < kMaxChain)
                pr.bank.chain[(size_t) pr.bank.chainLen++] = { (uint8_t) juce::jlimit (0, kPatterns - 1, (int) (*a)[0]), (uint8_t) juce::jlimit (1, 64, (int) (*a)[1]),
                                                                a->size() > 2 && (int) (*a)[2] != 0, (uint8_t) (a->size() > 3 ? juce::jlimit (0, 15, (int) (*a)[3]) : 0) };
    fxFromVar (v.getProperty ("fx", {}), pr.fx);
    if (withMidi && v.hasProperty ("midi")) midiFromVar (v.getProperty ("midi", {}), pr.midi); else pr.midi = keepMidi;
    pr.seed = (uint32_t) juce::jlimit (1, 1 << 30, (int) v.getProperty ("seed", 1));
}

} // namespace dh::dv::pio
