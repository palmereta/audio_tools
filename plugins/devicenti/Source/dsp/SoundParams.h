#pragma once
// DeviceNTI V: the sound. A sound is a flat array of floats, one per parameter in the table below. That one table is
// the whole sound model: the engine reads it, step locks address it by index, the LFO and modulation-matrix
// destinations are indexes into it, presets store it by key, and patch swap copies it. The keys are stable: the
// 3D panel of phase 2 and the saved state are built on them. Never reorder or reuse a key; add new ones at the end
// of a group only before release.
//
// A track owns nine sounds: sound 0 is the TONE voice (or, on a KIT track, the kit bus: its filter, drive, AGE,
// insert and sends act on the summed kit), sounds 1..8 are the eight KIT lanes. Each lane is a full voice (the same
// four-operator core with fixed-frequency operators, pitch sweep, noise, fold), so everything here applies to lanes.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace dh::dv {

enum PFlag : uint8_t { pfNone = 0, pfLog = 1, pfInt = 2, pfBip = 4 };

// Operator block (17 parameters), repeated for A, B, C, D.
#define DV_OP_PARAMS(X, L, l) \
    X (L##_RATIO,  l "_ratio",  #L " Ratio",          0.25f, 16.0f,   1.0f,  pfLog) \
    X (L##_DETUNE, l "_detune", #L " Detune",        -50.0f, 50.0f,   0.0f,  pfBip) \
    X (L##_LEVEL,  l "_level",  #L " Level",          0.0f,  1.0f,    0.0f,  pfNone) \
    X (L##_FB,     l "_fb",     #L " Feedback",       0.0f,  1.0f,    0.0f,  pfNone) \
    X (L##_HSHAPE, l "_hshape", #L " Harmonic shape", 0.0f,  5.0f,    0.0f,  pfInt) \
    X (L##_HAMT,   l "_hamt",   #L " Harmonics",      0.0f,  1.0f,    0.0f,  pfNone) \
    X (L##_ATT,    l "_att",    #L " Attack",         0.0f,  20.0f,   0.002f, pfLog) \
    X (L##_DEC,    l "_dec",    #L " Decay",          0.001f, 30.0f,  0.5f,  pfLog) \
    X (L##_SUS,    l "_sus",    #L " Sustain",        0.0f,  1.0f,    1.0f,  pfNone) \
    X (L##_REL,    l "_rel",    #L " Release",        0.002f, 30.0f,  0.2f,  pfLog) \
    X (L##_CURVE,  l "_curve",  #L " Curve",         -1.0f,  1.0f,    0.0f,  pfBip) \
    X (L##_DELAY,  l "_delay",  #L " Env delay",      0.0f,  5.0f,    0.0f,  pfNone) \
    X (L##_LOOP,   l "_loop",   #L " Env loop",       0.0f,  1.0f,    0.0f,  pfInt) \
    X (L##_VEL,    l "_vel",    #L " Velocity",       0.0f,  1.0f,    0.3f,  pfNone) \
    X (L##_KS,     l "_ks",     #L " Key scaling",   -1.0f,  1.0f,    0.0f,  pfBip) \
    X (L##_FIXED,  l "_fixed",  #L " Fixed Hz",       0.0f,  12000.0f, 0.0f, pfNone) \
    X (L##_TSCALE, l "_tscale", #L " Env time scale", 0.1f,  10.0f,   1.0f,  pfLog)

#define DV_LFO_PARAMS(X, N, l) \
    X (LFO##N##_SHAPE, l "_shape", "LFO " #N " Shape",  0.0f, 5.0f,   0.0f,  pfInt) \
    X (LFO##N##_RATE,  l "_rate",  "LFO " #N " Rate",   0.01f, 40.0f, 1.0f,  pfLog) \
    X (LFO##N##_SYNC,  l "_sync",  "LFO " #N " Sync",   0.0f, 21.0f,  0.0f,  pfInt) \
    X (LFO##N##_MODE,  l "_mode",  "LFO " #N " Mode",   0.0f, 3.0f,   0.0f,  pfInt) \
    X (LFO##N##_FADE,  l "_fade",  "LFO " #N " Fade",  -10.0f, 10.0f, 0.0f,  pfBip) \
    X (LFO##N##_PHASE, l "_phase", "LFO " #N " Phase",  0.0f, 1.0f,   0.0f,  pfNone) \
    X (LFO##N##_DEST,  l "_dest",  "LFO " #N " Destination", 0.0f, 255.0f, 0.0f, pfInt) \
    X (LFO##N##_DEPTH, l "_depth", "LFO " #N " Depth", -1.0f, 1.0f,   0.0f,  pfBip)

#define DV_MOD_PARAMS(X, N, l) \
    X (MOD##N##_SRC, l "_src", "Mod " #N " Source",      0.0f, 11.0f,  0.0f, pfInt) \
    X (MOD##N##_DST, l "_dst", "Mod " #N " Destination", 0.0f, 255.0f, 0.0f, pfInt) \
    X (MOD##N##_AMT, l "_amt", "Mod " #N " Amount",     -1.0f, 1.0f,   0.0f, pfBip)

#define DV_SOUND_PARAMS(X) \
    X (ALGO, "algo", "Algorithm", 0.0f, 7.0f, 0.0f, pfInt) \
    DV_OP_PARAMS (X, A, "a") DV_OP_PARAMS (X, B, "b") DV_OP_PARAMS (X, C, "c") DV_OP_PARAMS (X, D, "d") \
    X (PENV_DEPTH, "penv_depth", "Pitch env depth", -48.0f, 48.0f, 0.0f,  pfBip) \
    X (PENV_TIME,  "penv_time",  "Pitch env time",   0.0005f, 4.0f, 0.05f, pfLog) \
    X (NOISE_LEVEL,"noise_level","Noise level",      0.0f, 1.0f,  0.0f,    pfNone) \
    X (NOISE_DECAY,"noise_decay","Noise decay",      0.002f, 10.0f, 0.05f, pfLog) \
    X (NOISE_FREQ, "noise_freq", "Noise frequency",  40.0f, 18000.0f, 5000.0f, pfLog) \
    X (NOISE_Q,    "noise_q",    "Noise Q",          0.3f, 10.0f, 0.7f,    pfLog) \
    X (NOISE_MODE, "noise_mode", "Noise filter",     0.0f, 2.0f,  1.0f,    pfInt) \
    X (NOISE_ATT,  "noise_att",  "Noise attack",     0.0f, 4.0f,  0.0002f, pfNone) \
    X (NOISE_ENV,  "noise_env",  "Noise follows amp",0.0f, 1.0f,  0.0f,    pfInt) \
    X (DRIVE,      "drive",      "Drive",            0.0f, 1.0f,  0.0f,    pfNone) \
    X (FOLD,       "fold",       "Fold",             0.0f, 1.0f,  0.0f,    pfNone) \
    X (CRUSH,      "crush",      "Crush",            0.0f, 1.0f,  0.0f,    pfNone) \
    X (FTYPE,      "ftype",      "Filter type",      0.0f, 6.0f,  0.0f,    pfInt) \
    X (CUTOFF,     "cutoff",     "Cutoff",           20.0f, 20000.0f, 8000.0f, pfLog) \
    X (RESO,       "reso",       "Resonance",        0.0f, 1.0f,  0.1f,    pfNone) \
    X (FENV,       "fenv",       "Filter env",      -1.0f, 1.0f,  0.0f,    pfBip) \
    X (F_ATT,      "f_att",      "Filter attack",    0.0f, 20.0f, 0.002f,  pfLog) \
    X (F_DEC,      "f_dec",      "Filter decay",     0.001f, 30.0f, 0.3f,  pfLog) \
    X (F_SUS,      "f_sus",      "Filter sustain",   0.0f, 1.0f,  0.0f,    pfNone) \
    X (F_REL,      "f_rel",      "Filter release",   0.002f, 30.0f, 0.2f,  pfLog) \
    X (KTRK,       "ktrk",       "Keytrack",         0.0f, 1.0f,  0.3f,    pfNone) \
    X (BASE,       "base",       "Span base",        10.0f, 2000.0f, 10.0f, pfLog) \
    X (WIDTH,      "width",      "Span width",       500.0f, 22000.0f, 22000.0f, pfLog) \
    X (BASSCOMP,   "basscomp",   "Bass compensation",0.0f, 1.0f,  0.5f,    pfNone) \
    X (AMP_ATT,    "amp_att",    "Amp attack",       0.0f, 20.0f, 0.001f,  pfLog) \
    X (AMP_DEC,    "amp_dec",    "Amp decay",        0.001f, 30.0f, 0.3f,  pfLog) \
    X (AMP_SUS,    "amp_sus",    "Amp sustain",      0.0f, 1.0f,  1.0f,    pfNone) \
    X (AMP_REL,    "amp_rel",    "Amp release",      0.002f, 30.0f, 0.08f, pfLog) \
    X (VEL_AMP,    "vel_amp",    "Velocity to amp",  0.0f, 1.0f,  0.5f,    pfNone) \
    X (PAN,        "pan",        "Pan",             -1.0f, 1.0f,  0.0f,    pfBip) \
    X (LEVEL,      "level",      "Level",           -40.0f, 12.0f, 0.0f,   pfBip) \
    X (SUB_LEVEL,  "sub_level",  "Sub level",        0.0f, 1.0f,  0.0f,    pfNone) \
    X (SUB_OCT,    "sub_oct",    "Sub octave",       0.0f, 1.0f,  0.0f,    pfInt) \
    X (SUB_SHAPE,  "sub_shape",  "Sub shape",        0.0f, 1.0f,  0.0f,    pfInt) \
    X (UNISON,     "unison",     "Unison",           1.0f, 4.0f,  1.0f,    pfInt) \
    X (UNI_DETUNE, "uni_detune", "Unison detune",    0.0f, 50.0f, 10.0f,   pfNone) \
    X (UNI_SPREAD, "uni_spread", "Unison spread",    0.0f, 1.0f,  0.5f,    pfNone) \
    X (VMODE,      "vmode",      "Voice mode",       0.0f, 2.0f,  0.0f,    pfInt) \
    X (VOICES,     "voices",     "Max voices",       1.0f, 16.0f, 8.0f,    pfInt) \
    X (GLIDE,      "glide",      "Glide",            0.0f, 2.0f,  0.0f,    pfNone) \
    X (GLIDE_MODE, "glide_mode", "Glide always",     0.0f, 1.0f,  0.0f,    pfInt) \
    X (TUNE,       "tune",       "Tune",            -24.0f, 24.0f, 0.0f,   pfBip) \
    X (FINE,       "fine",       "Fine",            -100.0f, 100.0f, 0.0f, pfBip) \
    X (BEND,       "bend",       "Bend range",       1.0f, 24.0f, 2.0f,    pfInt) \
    X (AGE,        "age",        "AGE",              0.0f, 1.0f,  0.0f,    pfNone) \
    X (VINTAGE,    "vintage",    "Vintage voicing",  0.0f, 1.0f,  0.0f,    pfInt) \
    X (SEND_DELAY, "send_delay", "Delay send",       0.0f, 1.0f,  0.0f,    pfNone) \
    X (SEND_REVERB,"send_reverb","Reverb send",      0.0f, 1.0f,  0.0f,    pfNone) \
    X (INS_TYPE,   "ins_type",   "Insert",           0.0f, 6.0f,  0.0f,    pfInt) \
    X (INS_AMT,    "ins_amt",    "Insert amount",    0.0f, 1.0f,  0.5f,    pfNone) \
    X (INS_P1,     "ins_p1",     "Insert tone",      0.0f, 1.0f,  0.5f,    pfNone) \
    X (INS_P2,     "ins_p2",     "Insert character", 0.0f, 1.0f,  0.5f,    pfNone) \
    X (INS_MIX,    "ins_mix",    "Insert mix",       0.0f, 1.0f,  1.0f,    pfNone) \
    X (LOWMONO,    "lowmono",    "Low-end mono",     0.0f, 1.0f,  1.0f,    pfInt) \
    DV_LFO_PARAMS (X, 1, "lfo1") DV_LFO_PARAMS (X, 2, "lfo2") \
    DV_MOD_PARAMS (X, 1, "mod1") DV_MOD_PARAMS (X, 2, "mod2") DV_MOD_PARAMS (X, 3, "mod3") DV_MOD_PARAMS (X, 4, "mod4") \
    DV_MOD_PARAMS (X, 5, "mod5") DV_MOD_PARAMS (X, 6, "mod6") DV_MOD_PARAMS (X, 7, "mod7") DV_MOD_PARAMS (X, 8, "mod8") \
    X (STRUM,      "strum",      "Strum",            0.0f, 0.06f, 0.0f,    pfNone) \
    X (STRUM_DIR,  "strum_dir",  "Strum direction",  0.0f, 2.0f,  0.0f,    pfInt) \
    X (HUM_VEL,    "hum_vel",    "Humanise velocity",0.0f, 1.0f,  0.0f,    pfNone) \
    X (HUM_TIME,   "hum_time",   "Humanise timing",  0.0f, 1.0f,  0.0f,    pfNone) \
    X (CHORD_HOLD, "chord_hold", "Drone hold",       0.0f, 1.0f,  0.0f,    pfInt) \
    X (ARP_MODE,   "arp_mode",   "Arp mode",         0.0f, 5.0f,  0.0f,    pfInt) \
    X (ARP_RATE,   "arp_rate",   "Arp rate",         0.0f, 21.0f, 9.0f,    pfInt) \
    X (ARP_RANGE,  "arp_range",  "Arp octaves",      1.0f, 4.0f,  1.0f,    pfInt) \
    X (KIT_PITCH,  "kit_pitch",  "Lane pitch",       0.0f, 127.0f, 48.0f,  pfNone) \
    X (LANE_NOTE,  "lane_note",  "Lane MIDI note",   0.0f, 127.0f, 36.0f,  pfInt) \
    X (CHOKE,      "choke",      "Choke group",      0.0f, 3.0f,  0.0f,    pfInt)

enum SoundParam : int
{
   #define DV_ENUM(id, key, name, mn, mx, df, fl) id,
    DV_SOUND_PARAMS (DV_ENUM)
   #undef DV_ENUM
    kNumSoundParams
};

struct ParamInfo { const char* key; const char* name; float min, max, def; uint8_t flags; };

inline const std::array<ParamInfo, kNumSoundParams>& paramInfo()
{
    static const std::array<ParamInfo, kNumSoundParams> t {{
       #define DV_INFO(id, key, name, mn, mx, df, fl) { key, name, mn, mx, df, (uint8_t) (fl) },
        DV_SOUND_PARAMS (DV_INFO)
       #undef DV_INFO
    }};
    return t;
}

inline int paramIndex (const char* key)
{
    const auto& t = paramInfo();
    for (int i = 0; i < kNumSoundParams; ++i) if (std::strcmp (t[(size_t) i].key, key) == 0) return i;
    return -1;
}

// Operator parameter offsets inside a block, and the block of operator k (0..3 = A..D).
enum OpOff : int { oRatio, oDetune, oLevel, oFb, oHShape, oHAmt, oAtt, oDec, oSus, oRel, oCurve, oDelay, oLoop, oVel, oKs, oFixed, oTScale, kOpParams };
constexpr int opParam (int op, int off) { return A_RATIO + op * kOpParams + off; }
static_assert (B_RATIO - A_RATIO == kOpParams && D_TSCALE == A_RATIO + 4 * kOpParams - 1, "operator blocks");
static_assert (LFO2_SHAPE - LFO1_SHAPE == 8 && MOD2_SRC - MOD1_SRC == 3, "lfo / mod blocks");

// Global FX parameters that a step can lock (they act from the trig until its gate ends). Lock ids ≥ kGlobalLockBase.
enum GlobalLock : int { kGlobalLockBase = 400, gDelayFeedback = kGlobalLockBase, gDelayTime, gDelayFreeze, gReverbDecay, gReverbShimmer, gReverbFreeze, gTapeNoise, kGlobalLockEnd };
inline const char* globalLockKey (int id)
{
    static const char* k[] = { "g_delay_fb", "g_delay_time", "g_delay_freeze", "g_reverb_decay", "g_reverb_shimmer", "g_reverb_freeze", "g_tape_noise" };
    return id >= kGlobalLockBase && id < kGlobalLockEnd ? k[id - kGlobalLockBase] : "";
}
inline int lockIndex (const char* key)
{
    const int p = paramIndex (key);
    if (p >= 0) return p;
    for (int i = kGlobalLockBase; i < kGlobalLockEnd; ++i) if (std::strcmp (globalLockKey (i), key) == 0) return i;
    return -1;
}

// Names of the integer choices (for the display, presets and the manual).
inline const char* algoName (int a) { static const char* n[8] = { "STACK", "FORK", "PAIRS", "FAN", "CROWN", "STACK+SINE", "DIAMOND", "CHOIR" }; return n[a & 7]; }
inline const char* harmShapeName (int s) { static const char* n[6] = { "Sine", "Saw", "Square", "Organ", "Reed", "Bell" }; return n[s < 0 ? 0 : (s > 5 ? 5 : s)]; }
inline const char* filterName (int f) { static const char* n[7] = { "LP24", "LP12", "BP", "HP", "NOTCH", "PEAK", "OFF" }; return n[f < 0 ? 0 : (f > 6 ? 6 : f)]; }
inline const char* insertName (int i) { static const char* n[7] = { "Off", "Overdrive/Fold", "Bitcrush/Lo-fi", "Chorus/Ensemble", "Phaser", "Ring Mod", "Comb/Resonator" }; return n[i < 0 ? 0 : (i > 6 ? 6 : i)]; }
inline const char* lfoShapeName (int s) { static const char* n[6] = { "Sine", "Triangle", "Saw", "Square", "S&H", "Smooth random" }; return n[s < 0 ? 0 : (s > 5 ? 5 : s)]; }
inline const char* lfoModeName (int m) { static const char* n[4] = { "Free", "Trig", "Hold", "One-shot" }; return n[m & 3]; }
inline const char* modSourceName (int s)
{
    static const char* n[12] = { "None", "Velocity", "Keytrack", "Mod wheel", "Aftertouch", "Poly pressure", "Pitch bend", "LFO 1", "LFO 2", "Random", "Breath", "Expression" };
    return n[s < 0 ? 0 : (s > 11 ? 11 : s)];
}
enum ModSource : int { msNone, msVelocity, msKey, msWheel, msAftertouch, msPoly, msBend, msLfo1, msLfo2, msRandom, msBreath, msExpression, kNumModSources };

// LFO / matrix destinations: 0 none, 1-4 virtual, 5 + p = sound parameter p.
enum ModDest : int { mdNone = 0, mdPitch = 1, mdAmp = 2, mdIndex = 3, mdPanSpread = 4, kDestParamBase = 5 };
inline int destForParam (int p) { return kDestParamBase + p; }

// Tempo divisions for synced LFOs and the arpeggiator, in beats (index 0 = free for LFOs).
inline double divisionBeats (int i)
{
    static const double b[22] = { 0.0, 1.0 / 16, 1.0 / 12, 1.0 / 8, 1.0 / 6, 3.0 / 16, 0.25, 1.0 / 3, 0.375, 0.5, 2.0 / 3, 0.75,
                                  1.0, 1.5, 2.0, 3.0, 4.0, 8.0, 16.0, 32.0, 48.0, 64.0 };
    return b[i < 0 ? 0 : (i > 21 ? 21 : i)];
}
inline const char* divisionName (int i)
{
    static const char* n[22] = { "Free", "1/64", "1/32T", "1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D",
                                 "1/4", "1/4D", "1/2", "1/2D", "1 bar", "2 bars", "4 bars", "8 bars", "12 bars", "16 bars" };
    return n[i < 0 ? 0 : (i > 21 ? 21 : i)];
}

struct Sound
{
    std::array<float, kNumSoundParams> v {};
    Sound() { reset(); }
    void reset() { const auto& t = paramInfo(); for (int i = 0; i < kNumSoundParams; ++i) v[(size_t) i] = t[(size_t) i].def; v[A_LEVEL] = 1.0f; }
    float operator[] (int i) const { return v[(size_t) i]; }
    float& operator[] (int i) { return v[(size_t) i]; }
    int i (int p) const { return (int) (v[(size_t) p] + 0.5f); }
};

inline float clampParam (int p, float x)
{
    const auto& t = paramInfo()[(size_t) p];
    x = x < t.min ? t.min : (x > t.max ? t.max : x);
    if (t.flags & pfInt) x = (float) (int) (x + (x >= 0 ? 0.5f : -0.5f));
    return x;
}

// Normalised 0..1 position of a value (log parameters on a log scale), and back. Used by CC, LFO and matrix.
inline float toNorm (int p, float x)
{
    const auto& t = paramInfo()[(size_t) p];
    if ((t.flags & pfLog) && t.min > 0.0f) { const float a = std::log (t.min), b = std::log (t.max); return (std::log (x < t.min ? t.min : x) - a) / (b - a); }
    if ((t.flags & pfLog) && t.min <= 0.0f)
    {
        // Times that start at 0: a quadratic curve (fine control near 0).
        const float u = (x - t.min) / (t.max - t.min);
        return std::sqrt (u < 0 ? 0 : u);
    }
    return (x - t.min) / (t.max - t.min);
}
inline float fromNorm (int p, float u)
{
    const auto& t = paramInfo()[(size_t) p];
    u = u < 0 ? 0 : (u > 1 ? 1 : u);
    if ((t.flags & pfLog) && t.min > 0.0f) return std::exp (std::log (t.min) + u * (std::log (t.max) - std::log (t.min)));
    if ((t.flags & pfLog) && t.min <= 0.0f) return t.min + u * u * (t.max - t.min);
    return t.min + u * (t.max - t.min);
}

} // namespace dh::dv
