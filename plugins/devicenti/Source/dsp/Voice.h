#pragma once
// DeviceNTI V: one voice (plan §5.2, §5.3, §13.1). Everything up to the amp envelope runs at twice the host rate and is
// brought down by the voice's own half-band decimator (exactly 24 samples of delay):
//
//   four operators A..D (8 algorithms, own ADSR, feedback on any operator, harmonics morph, fixed-frequency mode)
//   + noise (white through its own state-variable filter, own decay or following the amp envelope)
//   → DRIVE (asymmetric soft clip, ADAA) → FOLD (triangle folder, ADAA) → CRUSH (bits and sample-and-hold)
//   → SPAN (6 dB base high-pass, 6 dB width low-pass) → filter (LP24 ladder / SVF modes) → amp envelope
//
// The SUB layer (sine or triangle one or two octaves under the note) runs at the host rate, follows the amp
// envelope and goes out on its own output, clean and mono: it never sees the drive, the filter or the insert. It is
// delayed by the decimator's 24 samples so it stays phase-locked to the FM layer.
//
// Control rate: every 16 host samples (aligned to the absolute sample count, so any block size gives the same
// result) the voice recomputes its targets from the sound, the step locks, the LFOs and the modulation matrix;
// inside the period every continuous value is interpolated per sample.
//
// Anti-aliasing at 2x (verified by devicentitest T-alias): every modulation index is limited per note with
// Carson's rule (destination frequency + (I + 1)·modulator frequency below 0.6·fs2), feedback is reduced with pitch,
// harmonics-morph partials above 18 kHz (carriers) / 30 kHz (modulators) are dropped, drive and fold use first-order
// antiderivative anti-aliasing.
//
// Killing a voice (steal, choke, retrigger of a mono lane) fades it with a 5 ms raised cosine (zero slope at both
// ends) and then flushes the decimator, so no corner and no truncated tail reach the output.

#include "SoundParams.h"
#include "Filter.h"

namespace dh::dv {

constexpr int kCtl = 16;                  // control period in host samples
constexpr int kMaxLocks = 12;

// Modulation edges: MOD[a][src][dst], src > dst always; carriers per algorithm.
struct Algo { bool mod[4][4]; bool car[4]; int nCar; };
inline const std::array<Algo, 8>& algos()
{
    static const std::array<Algo, 8> a = [] {
        std::array<Algo, 8> t {};
        const int edges[8][4][2] = {
            { { 3, 2 }, { 2, 1 }, { 1, 0 }, { -1, -1 } },      // STACK D>C>B>A
            { { 3, 1 }, { 2, 1 }, { 1, 0 }, { -1, -1 } },      // FORK (D+C)>B>A
            { { 3, 2 }, { 1, 0 }, { -1, -1 }, { -1, -1 } },    // PAIRS D>C, B>A
            { { 3, 2 }, { 3, 1 }, { 3, 0 }, { -1, -1 } },      // FAN D>(A,B,C)
            { { 3, 0 }, { 2, 0 }, { 1, 0 }, { -1, -1 } },      // CROWN (B+C+D)>A
            { { 3, 2 }, { 2, 1 }, { -1, -1 }, { -1, -1 } },    // STACK+SINE D>C>B, A alone
            { { 3, 2 }, { 3, 1 }, { 2, 0 }, { 1, 0 } },        // DIAMOND D>(B,C)>A
            { { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 } } // CHOIR
        };
        const int cars[8][4] = { { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 1, 0 }, { 1, 1, 1, 0 }, { 1, 0, 0, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 1, 1 } };
        for (int k = 0; k < 8; ++k)
        {
            for (auto& e : edges[k]) if (e[0] >= 0) t[(size_t) k].mod[e[0]][e[1]] = true;
            for (int o = 0; o < 4; ++o) { t[(size_t) k].car[o] = cars[k][o] != 0; t[(size_t) k].nCar += cars[k][o]; }
        }
        return t;
    }();
    return a;
}

// Harmonics shapes: partials 1..8, with the peak of each full shape for normalisation.
struct HarmShape { float w[8]; float peak; };
inline const std::array<HarmShape, 6>& harmShapes()
{
    static const std::array<HarmShape, 6> s = [] {
        const float raw[6][8] = {
            { 1, 0, 0, 0, 0, 0, 0, 0 },
            { 1, 1.0f / 2, 1.0f / 3, 1.0f / 4, 1.0f / 5, 1.0f / 6, 1.0f / 7, 1.0f / 8 },
            { 1, 0, 1.0f / 3, 0, 1.0f / 5, 0, 1.0f / 7, 0 },
            { 1, 0.7f, 0.0f, 0.45f, 0.0f, 0.25f, 0.0f, 0.18f },
            { 1, 0.15f, 0.6f, 0.1f, 0.4f, 0.05f, 0.25f, 0.0f },
            { 1, 0.0f, 0.0f, 0.55f, 0.0f, 0.0f, 0.0f, 0.35f } };
        std::array<HarmShape, 6> t {};
        for (int k = 0; k < 6; ++k)
        {
            double pk = 0;
            for (int i = 0; i < 2048; ++i)
            {
                double v = 0; const double ph = 2.0 * 3.14159265358979323846 * i / 2048.0;
                for (int h = 0; h < 8; ++h) v += raw[k][h] * std::sin ((h + 1) * ph);
                pk = std::max (pk, std::fabs (v));
            }
            for (int h = 0; h < 8; ++h) t[(size_t) k].w[h] = raw[k][h];
            t[(size_t) k].peak = (float) pk;
        }
        return t;
    }();
    return s;
}

// ADSR with delay and AD loop. Times are time-to-−60 dB (decay, release); the attack is an exponential approach
// to 1.3 that reaches 1 at the attack time (a convex rise, like an RC charging toward a higher rail).
struct Env
{
    enum Stage : uint8_t { idle, delay, attack, decay, release };
    uint8_t stage = idle; bool loop = false;
    float lev = 0.0f, ca = 1.0f, cd = 0.999f, cr = 0.999f, sus = 1.0f, curve = 0.0f;
    int delayLeft = 0;
    void set (float a, float d, float s, float r, float dly, float crv, bool lp, float fs)
    {
        a = std::max (a, 0.0003f); d = std::max (d, 0.0008f); r = std::max (r, 0.0015f);
        ca = 1.0f - std::exp (-1.466f / (a * fs));
        cd = std::exp (-6.9077553f / (d * fs));
        cr = std::exp (-6.9077553f / (r * fs));
        sus = clampf (s, 0.0f, 1.0f); curve = crv; loop = lp;
        delaySamples = (int) (dly * fs);
    }
    void trigger (bool fromZero)
    {
        if (fromZero) lev = 0.0f;
        delayLeft = delaySamples;
        stage = delayLeft > 0 ? delay : attack;
    }
    void gateOff() { if (stage != idle) stage = release; }
    inline float tick()
    {
        switch (stage)
        {
            case idle: return 0.0f;
            case delay: if (--delayLeft <= 0) stage = attack; break;
            case attack: lev += (1.3f - lev) * ca; if (lev >= 1.0f) { lev = 1.0f; stage = decay; } break;
            case decay:
                lev = sus + (lev - sus) * cd;
                if (loop && lev - sus < 0.03f * (1.0f - sus) + 0.002f) stage = attack;
                break;
            case release: lev *= cr; if (lev < 1.0e-6f) { lev = 0.0f; stage = idle; } break;
            default: break;
        }
        return lev + curve * lev * (1.0f - lev);
    }
    bool active() const { return stage != idle; }
private:
    int delaySamples = 0;
};

// A step lock captured by the voice at trig time.
struct Lock { int16_t id = -1; float v = 0.0f; };

// Track-level context for one control period (filled by the engine).
struct VoiceCtx
{
    double ppq = 0.0, bpm = 120.0; int64_t absSample = 0; bool playing = false;
    float wheel = 0, aftertouch = 0, bend = 0, breath = 0, expression = 0;   // 0..1 (bend −1..1)
    float kitDecayScale = 1.0f, kitAge = 0.0f;                               // KIT bus macros
    float groupGain[4] { 1, 1, 1, 1 };
    double fs = 48000.0;
};

struct VoiceOut { float* fmL = nullptr; float* fmR = nullptr; float* sub = nullptr; float* dL = nullptr; float* dR = nullptr; float* rL = nullptr; float* rR = nullptr; float* key = nullptr; };

constexpr int kQL = 8;                 // lanes per voice group (two NEON/SSE registers wide)
using VQ = V8;

// Per-sample state of the eight voice slots of a group, processed eight lanes at once (structure of arrays): the six
// envelopes of each voice (operators A-D, filter, amp), the 2x delay line that aligns undriven lanes with the 4x
// drive stage, the DC blocker, the SPAN one-poles, the state-variable filter, the ladder and the decimator.
struct QuadState
{
    // Envelopes: e = 0..3 operators, 4 filter, 5 amp. Stage codes as in Env.
    alignas (16) float eLev[6][kQL] {}, eCa[6][kQL] {}, eCd[6][kQL] {}, eCr[6][kQL] {}, eSus[6][kQL] {}, eCurve[6][kQL] {}, eStage[6][kQL] {}, eLoop[6][kQL] {}, eDelay[6][kQL] {}, eDelaySamples[6][kQL] {};
    alignas (16) float eP[6][kQL] {};                        // last host-rate value (for the half-way 2x value)
    alignas (16) float dl[32][kQL] {}; int dlp = 0;          // undriven lanes: the same 16-sample (2x) delay as the 4x stage
    alignas (16) float dcX[kQL] {}, dcY[kQL] {};
    alignas (16) float spHpZ[kQL] {}, spLpZ[kQL] {}, spHpA[kQL] {}, spLpA[kQL] {}, spHpOn[kQL] {}, spLpOn[kQL] {};
    alignas (16) float fType[kQL] {}, fRes[kQL] {}, fenvOct[kQL] {}, spr[4][kQL] {}, kSpr[kQL] {}, needCoefs[kQL] {};
    alignas (16) float sIc1[kQL] {}, sIc2[kQL] {}, sA1[kQL] {}, sA2[kQL] {}, sA3[kQL] {}, sK[kQL] {};
    alignas (16) float s1[kQL] {}, s2[kQL] {}, s3[kQL] {}, s4[kQL] {};
    alignas (16) float g0[kQL] {}, g1[kQL] {}, g2[kQL] {}, g3[kQL] {}, gt[kQL] {}, kk[kQL] {}, comp[kQL] {};
    alignas (16) float dh[2 * 128][kQL] {}; int dpos = 0;
    UpDownQ<kQL> ud; AdaaQ<kQL> adD, adF;                         // the 4x drive and fold stage (lanes that drive)
    void clearLane (int l)
    {
        for (int e = 0; e < 6; ++e) { eLev[e][l] = 0.0f; eStage[e][l] = 0.0f; eP[e][l] = 0.0f; eDelay[e][l] = 0.0f; }
        for (auto& r : dl) r[l] = 0.0f;
        dcX[l] = dcY[l] = spHpZ[l] = spLpZ[l] = sIc1[l] = sIc2[l] = 0.0f;
        s1[l] = s2[l] = s3[l] = s4[l] = 0.0f;
        for (auto& r : dh) r[l] = 0.0f;
        adD.primed[l] = adF.primed[l] = 0.0f;
    }
    // Move lane sl of S to lane dl of D (the group compaction): every per-lane value, the rings re-based on the
    // destination's positions so the lane's history reads the same.
    static void moveLane (QuadState& S, int sl, QuadState& D, int dl)
    {
        auto cp = [&] (float (*src)[kQL], float (*dst)[kQL], int rows) { for (int r = 0; r < rows; ++r) dst[r][dl] = src[r][sl]; };
        cp (S.eLev, D.eLev, 6); cp (S.eCa, D.eCa, 6); cp (S.eCd, D.eCd, 6); cp (S.eCr, D.eCr, 6); cp (S.eSus, D.eSus, 6);
        cp (S.eCurve, D.eCurve, 6); cp (S.eStage, D.eStage, 6); cp (S.eLoop, D.eLoop, 6); cp (S.eDelay, D.eDelay, 6);
        cp (S.eDelaySamples, D.eDelaySamples, 6); cp (S.eP, D.eP, 6); cp (S.spr, D.spr, 4);
        for (float* f : { S.dcX, S.dcY, S.spHpZ, S.spLpZ, S.spHpA, S.spLpA, S.spHpOn, S.spLpOn, S.fType, S.fRes, S.fenvOct, S.kSpr, S.needCoefs,
                          S.sIc1, S.sIc2, S.sA1, S.sA2, S.sA3, S.sK, S.s1, S.s2, S.s3, S.s4, S.g0, S.g1, S.g2, S.g3, S.gt, S.kk, S.comp,
                          S.adD.x1, S.adD.F1, S.adD.primed, S.adF.x1, S.adF.F1, S.adF.primed })
        {
            float* d = reinterpret_cast<float*> (reinterpret_cast<char*> (&D) + (reinterpret_cast<const char*> (f) - reinterpret_cast<const char*> (&S)));
            d[dl] = f[sl];
        }
        for (int k = 0; k < 32; ++k) D.dl[(D.dlp + k) & 31][dl] = S.dl[(S.dlp + k) & 31][sl];
        for (int k = 0; k < 128; ++k) { const float v = S.dh[(S.dpos + k) % 128][sl]; const int i = (D.dpos + k) % 128; D.dh[i][dl] = D.dh[i + 128][dl] = v; }
        for (int k = 0; k < 32; ++k) { const float v = S.ud.xh[(S.ud.xp + k) & 31][sl]; const int i = (D.ud.xp + k) & 31; D.ud.xh[i][dl] = D.ud.xh[i + 32][dl] = v; }
        for (int k = 0; k < 64; ++k) { const float v = S.ud.vh[(S.ud.vp + k) & 63][sl]; const int i = (D.ud.vp + k) & 63; D.ud.vh[i][dl] = D.ud.vh[i + 64][dl] = v; }
    }
};

// An envelope of a voice: a handle on its lane in the quad's arrays (set/trigger/release from the control code,
// ticked four lanes at once by the renderer).
struct EnvH
{
    QuadState* q = nullptr; int e = 0, l = 0;
    void set (float a, float d, float s, float r, float dly, float crv, bool lp, float fs)
    {
        a = std::max (a, 0.0003f); d = std::max (d, 0.0008f); r = std::max (r, 0.0015f);
        q->eCa[e][l] = 1.0f - std::exp (-1.466f / (a * fs));
        q->eCd[e][l] = std::exp (-6.9077553f / (d * fs));
        q->eCr[e][l] = std::exp (-6.9077553f / (r * fs));
        q->eSus[e][l] = clampf (s, 0.0f, 1.0f); q->eCurve[e][l] = crv; q->eLoop[e][l] = lp ? 1.0f : 0.0f;
        q->eDelaySamples[e][l] = (float) (int) (dly * fs);
    }
    void trigger (bool fromZero)
    {
        if (fromZero) q->eLev[e][l] = 0.0f;
        q->eDelay[e][l] = q->eDelaySamples[e][l];
        q->eStage[e][l] = q->eDelay[e][l] > 0.0f ? (float) Env::delay : (float) Env::attack;
    }
    void gateOff() { if (q->eStage[e][l] != (float) Env::idle) q->eStage[e][l] = (float) Env::release; }
    bool active() const { return q->eStage[e][l] != (float) Env::idle; }
    int stage() const { return (int) q->eStage[e][l]; }
    float lev() const { return q->eLev[e][l]; }
    float sus() const { return q->eSus[e][l]; }
    bool loop() const { return q->eLoop[e][l] > 0.0f; }
};

// Four envelopes (one per lane) one host sample forward: the same arithmetic as Env::tick, as selects.
template <class V, int NL> inline V envTick4 (QuadState& Q, int e)
{
    // Fast paths when no lane is in delay, attack or release (the usual case): same arithmetic, fewer selects.
    {
        const float* sp = Q.eStage[e];
        bool quiet = true, idle = true, loop = false;
        for (int l = 0; l < NL; ++l) { quiet &= sp[l] == 0.0f || sp[l] == 3.0f; idle &= sp[l] == 0.0f; loop |= Q.eLoop[e][l] > 0.0f; }
        if (quiet)
        {
            if (idle) return V (0.0f);
            if (! loop)
            {
                const V lev = V::load (Q.eLev[e]), sus = V::load (Q.eSus[e]), isDec = V::eq (V::load (sp), V (3.0f));
                const V nl = V::select (isDec, sus + (lev - sus) * V::load (Q.eCd[e]), lev);
                nl.store (Q.eLev[e]);
                return V::select (isDec, nl + V::load (Q.eCurve[e]) * nl * (V (1.0f) - nl), V (0.0f));
            }
        }
    }
    const V lev = V::load (Q.eLev[e]), st = V::load (Q.eStage[e]);
    const V isDelay = V::eq (st, V (1.0f)), isAtt = V::eq (st, V (2.0f)), isDec = V::eq (st, V (3.0f)), isRel = V::eq (st, V (4.0f));
    const V dly = V::load (Q.eDelay[e]) - isDelay;               // counts down only in the delay stage
    const V dlyDone = isDelay * V::lt (dly, V (0.5f));          // −−delayLeft ≤ 0
    const V la = lev + (V (1.3f) - lev) * V::load (Q.eCa[e]);
    const V attDone = isAtt * (V (1.0f) - V::lt (la, V (1.0f)));
    const V sus = V::load (Q.eSus[e]);
    const V ld = sus + (lev - sus) * V::load (Q.eCd[e]);
    const V loopRe = isDec * V::load (Q.eLoop[e]) * V::lt (ld - sus, V (0.03f) * (V (1.0f) - sus) + V (0.002f));
    const V lr = lev * V::load (Q.eCr[e]);
    const V relDone = isRel * V::lt (lr, V (1.0e-6f));
    V nl = V::select (isAtt, V::select (attDone, V (1.0f), la), V::select (isDec, ld, V::select (isRel, V::select (relDone, V (0.0f), lr), lev)));
    V ns = V::select (dlyDone, V (2.0f), V::select (attDone, V (3.0f), V::select (loopRe, V (2.0f), V::select (relDone, V (0.0f), st))));
    nl.store (Q.eLev[e]); ns.store (Q.eStage[e]); V::select (isDelay, dly, V::load (Q.eDelay[e])).store (Q.eDelay[e]);
    const V out = nl + V::load (Q.eCurve[e]) * nl * (V (1.0f) - nl);
    return V::select (V::eq (st, V (0.0f)), V (0.0f), out);      // idle at the start of the tick: 0
}

class Voice
{
public:
    QuadState* qs = nullptr; int ql = 0;          // this slot's lane in its group (set by the engine)
    int lid = -1;                                 // logical slot: the filter's analog spread and the iteration order
    static inline bool cullInaudible = true;      // voices whose output fell under −90 dB end early (off only for null tests)
    // After the engine moved this voice to another lane (the compaction): its envelope handles follow.
    void rebind (QuadState* q, int l)
    {
        qs = q; ql = l;
        for (int o = 0; o < 4; ++o) env[o] = { qs, o, ql };
        fenvE = { qs, 4, ql }; amp = { qs, 5, ql };
    }
    int ctlPos = 0;                               // host samples rendered since the last control()/start()
    static constexpr int kLatency = Decimator2::kLatency + UpDown4::kDelay2x / 2;    // 24 + 8 = 32 host samples
    // Identity, for the allocator.
    int track = -1, lane = 0, note = -1, group = -1, source = 0;   // source: 0 sequencer, 1 MIDI in, 2 arp
    int64_t startedAt = 0; uint32_t serial = 0;
    bool gate = false;

    void prepare (double sampleRate, uint32_t slotSeed)
    {
        fs = sampleRate; fs2 = 2.0 * sampleRate;
        filter.setSpread (slotSeed * 7919u + 13u);
        for (int o = 0; o < 4; ++o) env[o] = { qs, o, ql };
        fenvE = { qs, 4, ql }; amp = { qs, 5, ql };
        slot = slotSeed;
        reset();
    }
    void reset()
    {
        alive = false; fading = false; flush = 0; gate = false; track = -1; note = -1; group = -1;
        dec.reset(); filter.reset(); span.reset();
        std::fill (std::begin (subRing), std::end (subRing), 0.0f); subPos = 0;
        if (qs) qs->clearLane (ql);
    }
    bool isAlive() const { return alive || flush > 0; }
    bool isFading() const { return fading; }
    bool isReleased() const { return alive && ! gate; }
    float level() const { return amp.lev(); }

    struct Start
    {
        const Sound* snd = nullptr; int track = 0, lane = 0, note = 60, group = -1, source = 0;
        float pitch = 60.0f;            // MIDI pitch (chords, unison and KIT lane pitch already resolved)
        float velocity = 0.8f;          // 0..1
        float detuneCents = 0.0f, panOffset = 0.0f;
        int gateSamples = -1;           // −1: held until release()
        uint32_t seed = 1; int64_t now = 0; uint32_t serial = 0;
        const Lock* locks = nullptr; int nLocks = 0;
        bool perVoiceSends = false;     // KIT lanes send per voice
        float phaseSpread = 0.0f;       // unison: start-phase offset (turns)
    };

    void start (const Start& s, const VoiceCtx& c, int samplesToBoundary)
    {
        snd = s.snd; track = s.track; lane = s.lane; note = s.note; group = s.group; source = s.source;
        startedAt = s.now; serial = s.serial; seed = s.seed;
        vel = clampf (s.velocity, 0.0f, 1.0f);
        detune = s.detuneCents; panOff = s.panOffset;
        nLocks = std::min (s.nLocks, kMaxLocks);
        for (int i = 0; i < nLocks; ++i) locks[i] = s.locks[i];
        perVoiceSends = s.perVoiceSends;
        gateLeft = s.gateSamples; gate = true;
        const bool wasAlive = alive && ! fading;
        if (wasAlive) { for (int k = 0; k < kNumCur; ++k) cur[k] += inc[k] * (float) (2 * ctlPos); ctlPos = 0; }
        alive = true; fading = false; fadePos = 0; flush = 0;
        // Fresh voice: phases and filters from zero (a reused slot finished its fade and flush before).
        if (! wasAlive)
        {
            for (int o = 0; o < 4; ++o) { ph[o] = s.phaseSpread * (float) (o + 1); y1[o] = y2[o] = yv[o] = 0.0f; }
            filter.reset(); span.reset(); dcX = dcY = 0.0f; nz.seed (seed | 1u); noiseSvf.reset();
            if (lid >= 0) filter.setSpread ((uint32_t) (lid + 1) * 7919u + 13u);
            if (qs) qs->clearLane (ql);
            subPh = s.phaseSpread; crushHold = 0.0f; crushCount = 0;
            driven = false;
            pitchCur = s.pitch;
            for (auto& v : cur) v = 0.0f;
        }
        pitchTarget = s.pitch;
        if (! wasAlive) pitchCur = s.pitch;
        // Per-note AGE offsets (deterministic from the seed).
        ageNoteCents = hashBi (seed, 1, 21); ageCut = hashBi (seed, 2, 21); ageLev = hashBi (seed, 3, 21);
        for (int o = 0; o < 4; ++o) ageOp[o] = hashBi (seed, 10 + (uint64_t) o, 21);
        ageF1 = 0.08f + 0.17f * (float) hashUni (seed, 4, 21); ageF2 = 0.3f + 0.4f * (float) hashUni (seed, 5, 21);
        ageP1 = (float) hashUni (seed, 6, 21); ageP2 = (float) hashUni (seed, 7, 21);
        randomMod = hashBi (seed, 8, 21);
        noteTime = 0.0;
        lfoHeld[0] = lfoHeld[1] = 0.0f; lfoLatched[0] = lfoLatched[1] = false;
        penv = 1.0f;
        // Envelopes: retrigger (from the current level when the voice was sounding: no click on a mono retrigger).
        configure (c, true);
        for (auto& e : env) e.trigger (! wasAlive);
        fenvE.trigger (! wasAlive); amp.trigger (! wasAlive);
        noiseEnv = 1.0f; noiseRamp = 0.0f;
        firstTargets = ! wasAlive; coefCount = 0;
        targets (c, std::max (1, samplesToBoundary));
        ctlPos = 0;
        // A fresh voice starts on its targets; a retriggered one (mono, a reused slot still sounding) glides to
        // them over the control period: an instant jump of gain or cutoff on a sounding voice is a click.
        if (! wasAlive) for (int i = 0; i < kNumCur; ++i) { cur[i] = tgt[i]; inc[i] = 0.0f; }
    }

    // Mono legato: new pitch without retriggering the envelopes.
    void legatoTo (float pitch, int newNote, int gateSamples, const Lock* lk, int nLk, uint32_t newSerial)
    {
        pitchTarget = pitch; note = newNote; gateLeft = gateSamples; gate = true; serial = newSerial;
        nLocks = std::min (nLk, kMaxLocks);
        for (int i = 0; i < nLocks; ++i) locks[i] = lk[i];
        legatoGlide = true;
    }
    // A lock-only trig: its locks replace (or join) the sounding voice's locks.
    void legatoLocks (const Lock* lk, int nLk)
    {
        for (int i = 0; i < nLk; ++i)
        {
            bool done = false;
            for (int j = 0; j < nLocks && ! done; ++j) if (locks[j].id == lk[i].id) { locks[j].v = lk[i].v; done = true; }
            if (! done && nLocks < kMaxLocks) locks[nLocks++] = lk[i];
        }
    }
    void release() { if (! gate) return; gate = false; for (auto& e : env) e.gateOff(); fenvE.gateOff(); amp.gateOff(); }
    void kill() { if (! alive || fading) return; fading = true; fadePos = 0; }

    // Recompute targets at a control boundary (samples = host samples until the next boundary).
    void control (const VoiceCtx& c, int samples)
    {
        if (! alive) return;
        // The value reached at the end of the period just finished.
        for (int k = 0; k < kNumCur; ++k) cur[k] += inc[k] * (float) (2 * ctlPos);
        ctlPos = 0;
        // Silent voice: nothing can come back up (every carrier decays to zero or is released, no AD loop on a carrier,
        // the noise decays on its own) and what is left is below −90 dB: end it here instead of rendering its tail.
        // Poly voices only: a mono or legato voice still in its tail is what the next note reuses (its phases and
        // its glide start there), so it keeps sounding to the end as it always did.
        if (! fading && cullInaudible && P (VMODE) < 0.5f && inaudible()) { alive = false; flush = 32; return; }
        // The slow configuration (envelope coefficients, switches, modulation offsets) every 4th period (1.3 ms at
        // 48 kHz): it is the expensive part and nothing in it needs a faster rate.
        if (((c.absSample / kCtl) & 3) == 0) configure (c, false);
        else refreshMod (c);
        targets (c, samples);
        ctlPos = 0;
    }


    // Four voices at once (the slots of one quad), structure of arrays: the operator chain, the six envelopes, the DC
    // blocker, the SPAN one-poles, the state-variable filter or the ladder, the gains and the decimator run as four
    // lanes in SIMD registers; only lanes that need it run scalar code (noise, the 4x drive/fold stage, crush, sub,
    // the steal fade, filter coefficients every 4 samples). Each lane computes the single-voice arithmetic.
    template <class V, int NL> static void renderGroup (Voice* const* vv, const VoiceOut* outs, int n, int off, int64_t abs0)
    {
        Voice* v[kQL]; bool act[kQL], wasAct[kQL], any = false; QuadState* qs = nullptr;
        for (int l = 0; l < NL; ++l)
        {
            v[l] = vv[l];
            if (v[l] != nullptr) qs = v[l]->qs;
            act[l] = v[l] != nullptr && v[l]->alive;
            wasAct[l] = act[l];
            any |= act[l] || (v[l] != nullptr && v[l]->flush > 0);
        }
        if (! any || qs == nullptr) return;
        QuadState& Q = *qs;
        double v4fs2 = 96000.0;
        for (int l = 0; l < NL; ++l) if (v[l] != nullptr) v4fs2 = v[l]->fs2;
        alignas (16) float cur[kNumCur][kQL] {}, inc[kNumCur][kQL] {};
        V ph[4], y1[4], y2[4], yv[4], car[4], on[4], m1[4], m2[4], m3[4];
        alignas (16) float hw[4][8][kQL] {};
        bool anyH[4] {}, anyVint = false, vint[kQL] {}, special[kQL] {}, anySpecial = false, needRefresh = false;
        alignas (16) float isL[kQL] {}, actM[kQL] {}, tmp[4][4 * kQL] {};
        for (int l = 0; l < NL; ++l)
        {
            if (! act[l]) continue;
            Voice& w = *v[l];
            actM[l] = 1.0f;
            for (int k = 0; k < kNumCur; ++k) { cur[k][l] = w.cur[k]; inc[k][l] = w.inc[k]; }
            for (int o = 0; o < 4; ++o)
            {
                tmp[o][l] = w.ph[o]; tmp[o][kQL + l] = w.y1[o]; tmp[o][2 * kQL + l] = w.y2[o]; tmp[o][3 * kQL + l] = w.yv[o];
                if (w.hOn[o] && ! w.vintage) { anyH[o] = true; for (int k = 0; k < 8; ++k) hw[o][k][l] = w.hw[o][k]; }
                else hw[o][0][l] = 1.0f;
            }
            vint[l] = w.vintage; anyVint |= w.vintage;
            special[l] = w.noiseOn || w.driveOn || w.foldOn || w.driven || w.crushOn || w.subOn || w.fading;
            anySpecial |= special[l];
            isL[l] = w.ftype == kLP24 ? 1.0f : 0.0f;
            Q.fType[l] = (float) w.ftype; Q.fenvOct[l] = w.fenvOct;
            for (int st = 0; st < 4; ++st) Q.spr[st][l] = w.filter.stageSpread (st);
            Q.kSpr[l] = w.filter.resSpread(); Q.comp[l] = w.basscomp;
            if (w.coefCount == 0) { needRefresh = true; w.coefCount = 1; }
        }
        for (int o = 0; o < 4; ++o)
        {
            alignas (16) float a[kQL], b[kQL], c[kQL], d[kQL], e[kQL], f[kQL], g[kQL], hh[kQL], q[kQL];
            for (int l = 0; l < NL; ++l)
            {
                a[l] = tmp[o][l]; b[l] = tmp[o][kQL + l]; c[l] = tmp[o][2 * kQL + l]; d[l] = tmp[o][3 * kQL + l];
                const bool ok = act[l];
                const Algo& al = algos()[(size_t) (ok ? v[l]->algo : 0)];
                e[l] = ok && al.car[o] ? 1.0f : 0.0f; f[l] = ok && v[l]->opOn[o] ? 1.0f : 0.0f;
                g[l] = ok && o < 1 && al.mod[1][o] ? 1.0f : 0.0f; hh[l] = ok && o < 2 && al.mod[2][o] ? 1.0f : 0.0f; q[l] = ok && o < 3 && al.mod[3][o] ? 1.0f : 0.0f;
            }
            ph[o] = V::load (a); y1[o] = V::load (b); y2[o] = V::load (c); yv[o] = V::load (d); car[o] = V::load (e); on[o] = V::load (f);
            m1[o] = V::load (g); m2[o] = V::load (hh); m3[o] = V::load (q);
        }
        const V k2pi (1.0f / kTwoPi), half (0.5f), one (1.0f), ActM = V::load (actM), IsL = V::load (isL);
        bool ladderAny = false; for (int l = 0; l < NL; ++l) ladderAny |= isL[l] > 0.0f;
        const bool svfAny = [&] { for (int l = 0; l < NL; ++l) if (act[l] && isL[l] == 0.0f && v[l]->ftype != kFilterOff) return true; return false; }();
        // Continuous values are linear over a control period: start + increment · (2x samples since the period start).
        alignas (16) float boff[kQL] {};
        for (int l = 0; l < NL; ++l) boff[l] = act[l] ? (float) (2 * v[l]->ctlPos) : 0.0f;
        const V Boff = V::load (boff);
        V vAmp0[4], vInc0[4], vFb0[4], dAmp[4], dInc[4], dFb[4];
        for (int op = 0; op < 4; ++op)
        {
            vAmp0[op] = V::load (cur[cAmp0 + op]); dAmp[op] = V::load (inc[cAmp0 + op]);
            vInc0[op] = V::load (cur[cInc0 + op]); dInc[op] = V::load (inc[cInc0 + op]);
            vFb0[op] = V::load (cur[cFb0 + op]); dFb[op] = V::load (inc[cFb0 + op]);
        }
        const V vGain0 = V::load (cur[cGain]), dGain = V::load (inc[cGain]);
        alignas (16) float yBuf[kCtl][kQL] {}, sBuf[kCtl][kQL] {};
        V E[6], EP[6];
        for (int e = 0; e < 6; ++e) EP[e] = V::load (Q.eP[e]);
        for (int i = 0; i < n; ++i)
        {
            alignas (16) float x2[2][kQL] {}, subIn[kQL] {};
            for (int h = 0; h < 2; ++h)
            {
                const float jj = (float) (2 * i + h + 1);
                const V J = Boff + V (jj);
                if (h == 0)
                {
                    for (int l = 0; l < NL; ++l) if (act[l] && v[l]->gateLeft > 0 && --v[l]->gateLeft == 0) v[l]->release();
                    // Envelopes at the host rate; the 2x sample between two host samples takes the mean.
                    for (int e = 0; e < 6; ++e) { const V nv = envTick4<V, NL> (Q, e); E[e] = half * (EP[e] + nv); EP[e] = nv; }
                }
                else for (int e = 0; e < 6; ++e) E[e] = EP[e];
                V Amp[4], Inc[4], Fb[4];
                for (int op = 0; op < 4; ++op) { Amp[op] = vAmp0[op] + dAmp[op] * J; Inc[op] = vInc0[op] + dInc[op] * J; Fb[op] = vFb0[op] + dFb[op] * J; }
                // The operator chain.
                for (int op = 3; op >= 0; --op)
                {
                    V pm (0.0f);
                    if (op < 1) pm = pm + m1[op] * yv[1];
                    if (op < 2) pm = pm + m2[op] * yv[2];
                    if (op < 3) pm = pm + m3[op] * yv[3];
                    pm = pm + Fb[op] * half * (y1[op] + y2[op]);
                    const V th = ph[op] + pm * k2pi;
                    V w4;
                    if (anyH[op] || anyVint)
                    {
                        alignas (16) float ts[kQL], ws[kQL]; th.store (ts);
                        const float* sinTab = SineTable::get().t;
                        for (int l = 0; l < NL; ++l)
                        {
                            if (vint[l]) { const int idx = (int) std::floor ((ts[l] - std::floor (ts[l])) * 1024.0f) & 1023; ws[l] = sinTab[idx * 4]; continue; }
                            const float sn = sinT (ts[l]);
                            if (! anyH[op]) { ws[l] = sn; continue; }
                            const float cs = cosT (ts[l]);
                            float sp = 0.0f, sk = sn, ww = hw[op][0][l] * sn;
                            for (int k = 1; k < 8; ++k) { const float nx = 2.0f * cs * sk - sp; sp = sk; sk = nx; ww += hw[op][k][l] * sk; }
                            ws[l] = ww;
                        }
                        w4 = V::load (ws);
                    }
                    else w4 = sinT4 (th);
                    V raw = w4 * E[op];
                    if (anyVint)
                    {
                        alignas (16) float rs[kQL]; raw.store (rs);
                        for (int l = 0; l < NL; ++l) if (vint[l]) rs[l] = std::floor (rs[l] * 2048.0f + 0.5f) * (1.0f / 2048.0f);
                        raw = V::load (rs);
                    }
                    y2[op] = V::select (on[op], y1[op], y2[op]);
                    y1[op] = V::select (on[op], raw, y1[op]);
                    yv[op] = on[op] * V::select (car[op], raw, raw * Amp[op]);
                    const V np = ph[op] + Inc[op];
                    ph[op] = V::select (on[op], np - V::floor (np), ph[op]);
                }
                V x (0.0f);
                for (int op = 0; op < 4; ++op) x = x + car[op] * yv[op] * Amp[op];
                x = x * ActM;
                // Noise (lanes that have it), then the 2x delay line every lane goes through (undriven lanes take its
                // output; driven lanes run their own 4x stage on the same input), then crush.
                alignas (16) float xs[kQL], xd[kQL];
                if (anySpecial)
                {
                    x.store (xs);
                    for (int l = 0; l < NL; ++l) if (special[l] && act[l]) xs[l] = v[l]->noiseIn (xs[l], cur, inc, boff[l] + jj, l);
                    x = V::load (xs);
                }
                x.store (Q.dl[Q.dlp]);
                V y = V::load (Q.dl[(Q.dlp - UpDown4::kDelay2x) & 31]);
                Q.dlp = (Q.dlp + 1) & 31;
                if (anySpecial)
                {
                    alignas (16) float dM[kQL] {}, fM[kQL] {}, uM[kQL] {};
                    bool anyDrive = false, anyDriveOn = false, anyFoldOn = false;
                    for (int l = 0; l < NL; ++l)
                    {
                        if (! (special[l] && act[l])) continue;
                        Voice& w = *v[l];
                        if (w.driveOn || w.foldOn)
                        {
                            if (! w.driven)
                            {
                                // Starting the 4x stage: its histories as if it had seen the last 32 inputs (the quad's delay line).
                                float last[32];
                                for (int k = 0; k < 32; ++k) last[k] = Q.dl[(Q.dlp - 33 + k) & 31][l];
                                Q.ud.rebuildLane (l, last);
                                w.driven = true;
                            }
                            uM[l] = 1.0f; dM[l] = w.driveOn ? 1.0f : 0.0f; fM[l] = w.foldOn ? 1.0f : 0.0f; anyDrive = true;
                            anyDriveOn |= w.driveOn; anyFoldOn |= w.foldOn;
                        }
                        else w.driven = false;
                    }
                    y.store (xd);
                    if (anyDrive)
                    {
                        const V DM = V::load (dM), FM = V::load (fM), UM = V::load (uM);
                        const V gD = V::load (cur[cDrive]) + V::load (inc[cDrive]) * J, b = V::load (cur[cBias]) + V::load (inc[cBias]) * J;
                        const V gF = V::select (FM, V::load (cur[cFold]) + V::load (inc[cFold]) * J, V (1.0f));
                        const V comp = V::load (cur[cDriveComp]) + V::load (inc[cDriveComp]) * J;
                        const V cb = clipF4 (b), fk = V (0.75f) + V (0.25f) / gF;
                        const V out = Q.ud.process (V::load (xs), [&] (V& va, V& vb) {
                            if (anyDriveOn)
                            {
                                V da = gD * va + b, db = gD * vb + b;
                                Q.adD.process2 (da, db, clipF4<V>, clipAF4<V>);
                                va = V::select (DM, (da - cb) * comp, va); vb = V::select (DM, (db - cb) * comp, vb);
                            }
                            if (anyFoldOn)
                            {
                                V fa = gF * va, fb = gF * vb;
                                Q.adF.process2 (fa, fb, foldF4<V>, foldAF4<V>);
                                va = V::select (FM, fa * fk, va); vb = V::select (FM, fb * fk, vb);
                            } });
                        // Lanes not driving (or not folding) start unprimed next time.
                        (V::load (Q.adD.primed) * DM).store (Q.adD.primed);
                        (V::load (Q.adF.primed) * FM).store (Q.adF.primed);
                        V::select (UM, out, y).store (xd);
                    }
                    else { V (0.0f).store (Q.adD.primed); V (0.0f).store (Q.adF.primed); }
                    for (int l = 0; l < NL; ++l) if (special[l] && act[l]) xd[l] = v[l]->crush (xd[l]);
                    y = V::load (xd);
                }
                // DC blocker (≈ 10 Hz): FM itself makes DC when a sideband lands on 0 Hz (1:1 ratios), drive and crush add more.
                { const V dcx = V::load (Q.dcX), dcy = V::load (Q.dcY); const V dy = y - dcx + V (0.99935f) * dcy; y.store (Q.dcX); dy.store (Q.dcY); y = dy; }
                // SPAN: 6 dB base high-pass, 6 dB width low-pass.
                {
                    const V hpOn = V::load (Q.spHpOn), lpOn = V::load (Q.spLpOn);
                    V hz = V::load (Q.spHpZ); const V hzN = y + V::load (Q.spHpA) * (hz - y);
                    hz = V::select (hpOn, hzN, hz); hz.store (Q.spHpZ);
                    y = V::select (hpOn, y - hz, y);
                    V lz = V::load (Q.spLpZ); const V lzN = y + V::load (Q.spLpA) * (lz - y);
                    lz = V::select (lpOn, lzN, lz); lz.store (Q.spLpZ);
                    y = V::select (lpOn, lz, y);
                }
                // Filter coefficients, four lanes at once, every 4 samples (and at a note's first sample).
                if (((2 * (abs0 + i) + h) & 3) == 0 || needRefresh)     // every 4 samples of the absolute 2x timeline
                {
                    needRefresh = false;
                    const V oct = V::min (V (15.2f), V::max (V (4.32f), V::load (cur[cCutOct]) + V::load (inc[cCutOct]) * J + V::load (Q.fenvOct) * E[4]));
                    alignas (16) float oc[kQL], fc[kQL];
                    oct.store (oc);
                    for (int l = 0; l < NL; ++l) fc[l] = std::min (fastExp2 (oc[l]), (float) (0.42 * v4fs2));
                    const V g = fastTan4 (V (kPi) * V::load (fc) * V ((float) (1.0 / v4fs2)));
                    const V res = V::load (cur[cReso]) + V::load (inc[cReso]) * J;
                    res.store (Q.fRes);
                    if (ladderAny)
                    {
                        const V g0 = g * V::load (Q.spr[0]), g1 = g * V::load (Q.spr[1]), g2 = g * V::load (Q.spr[2]), g3 = g * V::load (Q.spr[3]);
                        const V G0 = g0 / (one + g0), G1 = g1 / (one + g1), G2 = g2 / (one + g2), G3 = g3 / (one + g3);
                        G0.store (Q.g0); G1.store (Q.g1); G2.store (Q.g2); G3.store (Q.g3);
                        (G0 * G1 * G2 * G3).store (Q.gt);
                        (V (3.9f) * res * V::load (Q.kSpr)).store (Q.kk);
                    }
                    if (svfAny)
                    {
                        const V k = V (2.0f) - V (1.96f) * res;
                        const V a1 = one / (one + g * (g + k)), a2 = g * a1, a3 = g * a2;
                        a1.store (Q.sA1); a2.store (Q.sA2); a3.store (Q.sA3); k.store (Q.sK);
                    }
                }
                const V pre = y;
                if (svfAny)
                {
                    const V ic1 = V::load (Q.sIc1), ic2 = V::load (Q.sIc2), a1 = V::load (Q.sA1), a2 = V::load (Q.sA2), a3 = V::load (Q.sA3), K = V::load (Q.sK);
                    const V v3 = pre - ic2;
                    const V v1 = a1 * ic1 + a2 * v3;
                    const V v2 = ic2 + a2 * ic1 + a3 * v3;
                    const V nIc1 = V (2.0f) * v1 - ic1, nIc2 = V (2.0f) * v2 - ic2;
                    const V isS = (one - IsL) * ActM * (one - V::eq (V::load (Q.fType), V ((float) kFilterOff)));
                    V::select (isS, fastTanh4 (nIc1 * V (0.4f)) * V (2.5f), ic1).store (Q.sIc1);
                    V::select (isS, nIc2, ic2).store (Q.sIc2);
                    const V lp = v2, hp = pre - K * v1 - v2, res = V::load (Q.fRes), t = V::load (Q.fType);
                    const V bp = v1 * K * (one + V (0.5f) * res);
                    const V pk = pre + v1 * K * (one + V (6.0f) * res);
                    V o = V::select (V::eq (t, V ((float) kLP12)), lp, V::select (V::eq (t, V ((float) kBP)), bp, V::select (V::eq (t, V ((float) kHP)), hp,
                           V::select (V::eq (t, V ((float) kNotch)), lp + hp, V::select (V::eq (t, V ((float) kPeak)), pk, pre)))));
                    y = V::select (isS, o, y);
                }
                if (ladderAny)
                {
                    const V s1 = V::load (Q.s1), s2 = V::load (Q.s2), s3 = V::load (Q.s3), s4 = V::load (Q.s4);
                    const V G0 = V::load (Q.g0), G1 = V::load (Q.g1), G2 = V::load (Q.g2), G3 = V::load (Q.g3), Gt = V::load (Q.gt), K = V::load (Q.kk);
                    const V S = ((s1 * (one - G0) * G1 + s2 * (one - G1)) * G2 + s3 * (one - G2)) * G3 + s4 * (one - G3);
                    const V in = pre * (one + V::load (Q.comp) * K);
                    const V y4e = (Gt * in + S) / (one + K * Gt);
                    const V u = fastTanh4 (in - K * y4e);
                    V vv_ = (u - s1) * G0; V o1 = vv_ + s1; const V n1 = o1 + vv_; o1 = softStage4 (o1);
                    vv_ = (o1 - s2) * G1; V o2 = vv_ + s2; const V n2 = o2 + vv_; o2 = softStage4 (o2);
                    vv_ = (o2 - s3) * G2; const V o3 = vv_ + s3; const V n3 = o3 + vv_;
                    vv_ = (o3 - s4) * G3; const V o4 = vv_ + s4; const V n4 = o4 + vv_;
                    const V L = IsL * ActM;
                    V::select (L, n1, s1).store (Q.s1); V::select (L, n2, s2).store (Q.s2); V::select (L, n3, s3).store (Q.s3); V::select (L, n4, s4).store (Q.s4);
                    y = V::select (L, o4, y);
                }
                // Amp, then (lanes that have them) the sub and the steal fade.
                const V Gain = vGain0 + dGain * J;
                y = y * (E[5] * Gain);
                if (anySpecial)
                {
                    alignas (16) float ys[kQL], av[kQL], gv[kQL]; y.store (ys); E[5].store (av); Gain.store (gv);
                    for (int l = 0; l < NL; ++l) if (special[l] && act[l]) ys[l] = v[l]->subFade (ys[l], av[l], cur, inc, boff[l] + jj, l, h, subIn[l]);
                    y = V::load (ys);
                }
                (y * ActM).store (x2[h]);
            }
            // The decimator, four lanes.
            const auto& hc = HalfbandCoefs::get();
            for (int h = 0; h < 2; ++h)
            {
                Q.dpos = (Q.dpos == 0 ? 128 : Q.dpos) - 1;
                const V xv = V::load (x2[h]);
                xv.store (Q.dh[Q.dpos]); xv.store (Q.dh[Q.dpos + 128]);
            }
            const float (*P)[kQL] = Q.dh + Q.dpos;
            V a0 = V (0.5f) * V::load (P[49]), a1 (0.0f), a2 (0.0f), a3 (0.0f);
            for (int j = 0; j < HalfbandCoefs::kPairs; j += 4)
            {
                a0 = a0 + V (hc.h[j]) * (V::load (P[48 - 2 * j]) + V::load (P[50 + 2 * j]));
                a1 = a1 + V (hc.h[j + 1]) * (V::load (P[46 - 2 * j]) + V::load (P[52 + 2 * j]));
                a2 = a2 + V (hc.h[j + 2]) * (V::load (P[44 - 2 * j]) + V::load (P[54 + 2 * j]));
                a3 = a3 + V (hc.h[j + 3]) * (V::load (P[42 - 2 * j]) + V::load (P[56 + 2 * j]));
            }
            alignas (16) float ys[kQL];
            ((a0 + a1) + (a2 + a3)).store (ys);
            for (int l = 0; l < NL; ++l)
            {
                if (v[l] == nullptr) continue;
                Voice& w = *v[l];
                if (act[l])
                {
                    yBuf[i][l] = ys[l]; sBuf[i][l] = w.popSub (subIn[l]);
                    if (w.fading && (float) w.fadePos >= w.fadeLen)
                    {
                        // Fade done: the voice ends; its decimator lane flushes (zeros in) for the next 32 samples.
                        w.alive = false; w.flush = 32; act[l] = false; actM[l] = 0.0f;
                        w.storeLaneV (l, ph, y1, y2, yv);
                        for (int k = 0; k < kNumCur; ++k) w.cur[k] = cur[k][l] + inc[k][l] * (boff[l] + (float) (2 * i + 2));
                        w.ctlPos = 0; w.lastOut = i + 1;
                    }
                }
                else if (! w.alive && w.flush > 0)
                {
                    w.outLane (outs[l], off + i, ys[l], w.popSub (0.0f), w.cur[cGl], w.cur[cGr], w.cur[cSendD], w.cur[cSendR]);
                    --w.flush;
                }
            }
        }
        for (int e = 0; e < 6; ++e) EP[e].store (Q.eP[e]);
        // The sounding lanes' samples into their tracks (pan and sends interpolated like the other values).
        for (int l = 0; l < NL; ++l)
        {
            if (v[l] == nullptr || ! wasAct[l]) continue;
            Voice& w = *v[l];
            const int m = act[l] ? n : w.lastOut;
            const VoiceOut& o = outs[l];
            for (int i = 0; i < m; ++i)
            {
                const float jj = boff[l] + (float) (2 * i + 2);
                const float gl = cur[cGl][l] + inc[cGl][l] * jj, gr = cur[cGr][l] + inc[cGr][l] * jj;
                const float y = yBuf[i][l], ll = y * gl, rr = y * gr;
                o.fmL[off + i] += ll; o.fmR[off + i] += rr;
                if (o.sub) o.sub[off + i] += sBuf[i][l];
                if (o.key) o.key[off + i] += y;
                if (w.perVoiceSends)
                {
                    const float sd = cur[cSendD][l] + inc[cSendD][l] * jj, sr = cur[cSendR][l] + inc[cSendR][l] * jj;
                    if (o.dL) { o.dL[off + i] += ll * sd; o.dR[off + i] += rr * sd; }
                    if (o.rL) { o.rL[off + i] += ll * sr; o.rR[off + i] += rr * sr; }
                }
            }
        }
        for (int l = 0; l < NL; ++l)
        {
            if (! act[l]) continue;
            Voice& w = *v[l];
            w.storeLaneV (l, ph, y1, y2, yv);
            w.ctlPos += n;
            w.noteTime += n / w.fs;
            if (! w.fading && ! w.amp.active()) { w.alive = false; w.flush = 32; }
        }
    }

    template <class V> static inline V softStage4 (V v)
    {
        const V a = V::abs (v);
        const V sat = V::copysign (V (0.7f) + V (0.3f) * fastTanh4 ((a - V (0.7f)) / V (0.3f)), v);
        return V::select (V::lt (a, V (0.7f)), v, sat);
    }

    template <class V> void storeLaneV (int l, const V* ph_, const V* y1_, const V* y2_, const V* yv_)
    {
        alignas (16) float t[kQL];
        for (int o = 0; o < 4; ++o)
        {
            ph_[o].store (t); ph[o] = t[l]; y1_[o].store (t); y1[o] = t[l]; y2_[o].store (t); y2[o] = t[l]; yv_[o].store (t); yv[o] = t[l];
        }
    }

    int lastOut = 0;

    // Lane helpers (scalar, only for lanes that need them).
    inline float noiseIn (float x, const float (*cv)[kQL], const float (*iv)[kQL], float jj, int l)
    {
        if (! noiseOn) return x;
        noiseSvf.tick (nz.bi());
        const float nf = noiseMode == 0 ? noiseSvf.lp : (noiseMode == 1 ? noiseSvf.bp * noiseSvf.k : noiseSvf.hp);
        noiseRamp = std::min (1.0f, noiseRamp + noiseRampInc);
        float ne;
        if (noiseFollowsAmp) ne = amp.lev(); else { ne = noiseEnv; noiseEnv *= noiseDecay; }
        return x + (cv[cNoise][l] + iv[cNoise][l] * jj) * ne * noiseRamp * nf;
    }
    inline float crush (float xd)
    {
        if (crushOn)
        {
            if (++crushCount >= crushN) { crushCount = 0; crushHold = std::floor (xd * crushQ + 0.5f) / crushQ; }
            xd = crushHold;
        }
        return xd;
    }
    inline float subFade (float x, float a, const float (*cv)[kQL], const float (*iv)[kQL], float jj, int l, int h, float& subIn)
    {
        if (h == 0 && subOn)
        {
            subPh += cv[cSubInc][l] + iv[cSubInc][l] * jj; subPh -= std::floor (subPh);
            const float sv = subTri ? (subPh < 0.5f ? 4.0f * subPh - 1.0f : 3.0f - 4.0f * subPh) : sinT (subPh);
            subIn = sv * a * (cv[cSubGain][l] + iv[cSubGain][l] * jj);
        }
        if (fading)
        {
            const float u = (float) fadePos / fadeLen;
            const float fg = u >= 1.0f ? 0.0f : 0.5f + 0.5f * cosT (0.5f * u);
            x *= fg; if (h == 0) subIn *= fg;
            ++fadePos;
        }
        return x;
    }

    float panNow() const { return cur[cPan]; }

    bool inaudible() const
    {
        const Algo& al = algos()[(size_t) algo];
        float est = 0.0f;
        const bool ampFalling = amp.stage() == Env::release || (amp.stage() == Env::decay && amp.sus() < 1e-4f);
        bool carriersFalling = true;
        for (int o = 0; o < 4; ++o)
        {
            if (! al.car[o] || ! opOn[o]) continue;
            const EnvH& e = env[o];
            const bool falling = e.stage() == Env::idle || e.stage() == Env::release || (e.stage() == Env::decay && e.sus() < 1e-4f && ! e.loop());
            carriersFalling &= falling;
            est += e.lev() * std::fabs (cur[cAmp0 + o]);
        }
        if (! (carriersFalling || ampFalling)) return false;
        est *= amp.lev() * std::fabs (cur[cGain]);
        if (noiseOn) { if (noiseFollowsAmp) est += amp.lev() * cur[cNoise] * std::fabs (cur[cGain]); else if (noiseEnv > 1e-4f || noiseRamp < 1.0f) return false; }
        if (subOn) { if (! ampFalling) return false; est += amp.lev() * cur[cSubGain]; }
        return est < 3.0e-5f;
    }

private:
    double fs = 48000.0, fs2 = 96000.0;
    const Sound* snd = nullptr;
    uint32_t seed = 1, slot = 0;
    bool alive = false, fading = false, perVoiceSends = false, legatoGlide = false, firstTargets = true;
    int flush = 0, fadePos = 0, gateLeft = -1; float fadeLen = 960.0f;
    float vel = 0.8f, detune = 0.0f, panOff = 0.0f;
    Lock locks[kMaxLocks]; int nLocks = 0;

    // Operator state.
    float ph[4] {}, y1[4] {}, y2[4] {}, yv[4] {};
    EnvH env[4], fenvE, amp;
    bool driven = false;                             // the 4x drive/fold stage was active on the last sample
    bool opOn[4] {}, hOn[4] {}; float hw[4][8] {}; float fbAmt[4] {};
    int algo = 0; bool vintage = false;
    // Noise.
    Noise nz; Svf noiseSvf; bool noiseOn = false, noiseFollowsAmp = false; int noiseMode = 1;
    float noiseEnv = 1.0f, noiseDecay = 0.999f, noiseRamp = 0.0f, noiseRampInc = 1.0f;
    // Shaping stages.
    bool driveOn = false, foldOn = false, crushOn = false; float dcX = 0, dcY = 0;
    int crushN = 1, crushCount = 0; float crushQ = 32768.0f, crushHold = 0.0f;
    Span span; bool spanHp = false, spanLp = false; float spanHpA = 0, spanLpA = 0;
    VoiceFilter filter; int ftype = 0; int modMask[4] {}; unsigned coefCount = 0, ctlCount = 0; float basscomp = 0.5f, fenvOct = 0.0f;
    // Sub.
    bool subOn = false, subTri = false; float subPh = 0.0f;
    float subRing[64] {}; int subPos = 0;
    Decimator2 dec;
    // Pitch.
    float pitchCur = 60.0f, pitchTarget = 60.0f, penv = 1.0f;
    double noteTime = 0.0;
    float ageNoteCents = 0, ageCut = 0, ageLev = 0, ageOp[4] {}, ageF1 = 0.1f, ageF2 = 0.5f, ageP1 = 0, ageP2 = 0, randomMod = 0;
    float lfoHeld[2] {}; bool lfoLatched[2] {};

    // Interpolated continuous values.
    enum Cur : int { cInc0, cInc1, cInc2, cInc3, cAmp0, cAmp1, cAmp2, cAmp3, cFb0, cFb1, cFb2, cFb3, cCutOct, cReso, cGain, cDrive, cBias, cDriveComp, cFold,
                     cNoise, cSubInc, cSubGain, cPan, cSendD, cSendR, cGl, cGr, kNumCur };
    float cur[kNumCur] {}, tgt[kNumCur] {}, inc[kNumCur] {};

    inline float popSub (float x)
    {
        // The voice latency (4x stage + decimator) so the sub lines up with the FM layer.
        subRing[subPos] = x; const float y = subRing[(subPos - kLatency) & 63]; subPos = (subPos + 1) & 63;
        return y;
    }
    inline void outLane (const VoiceOut& o, int idx, float y, float sub, float gl, float gr, float sd, float sr)
    {
        const float l = y * gl, r = y * gr;
        o.fmL[idx] += l; o.fmR[idx] += r;
        if (o.sub) o.sub[idx] += sub;
        if (o.key) o.key[idx] += y;
        if (perVoiceSends)
        {
            if (o.dL) { o.dL[idx] += l * sd; o.dR[idx] += r * sd; }
            if (o.rL) { o.rL[idx] += l * sr; o.rR[idx] += r * sr; }
        }
    }
    inline void outSample (const VoiceOut& o, int idx, float y, float sub)
    {
        // Constant-power pan (pan interpolated at control rate).
        const float l = y * cur[cGl], r = y * cur[cGr];
        o.fmL[idx] += l; o.fmR[idx] += r;
        if (o.sub) o.sub[idx] += sub;
        if (o.key) o.key[idx] += y;
        if (perVoiceSends)
        {
            if (o.dL) { o.dL[idx] += l * cur[cSendD]; o.dR[idx] += r * cur[cSendD]; }
            if (o.rL) { o.rL[idx] += l * cur[cSendR]; o.rR[idx] += r * cur[cSendR]; }
        }
    }

    // Base value with the step locks.
    inline float base (int p) const
    {
        for (int i = 0; i < nLocks; ++i) if (locks[i].id == p) return locks[i].v;
        return (*snd)[p];
    }

    // Modulation offsets of this control period (normalised units per parameter, plus the virtual destinations).
    struct Off { int dest; float amt; };
    Off offs[12]; int nOffs = 0;
    float vPitch = 0, vAmp = 0, vIndex = 0, vPan = 0;
    // The effective sound of this control period: the sound, the step locks, the modulation offsets (refreshMod).
    std::array<float, kNumSoundParams> eff {};
    inline float P (int p) const { return eff[(size_t) p]; }
    void buildEff()
    {
        eff = snd->v;
        for (int i = 0; i < nLocks; ++i) if (locks[i].id >= 0 && locks[i].id < kNumSoundParams) eff[(size_t) locks[i].id] = locks[i].v;
        for (int i = 0; i < nOffs; ++i)
        {
            const int p = offs[i].dest - kDestParamBase;
            const auto& t = paramInfo()[(size_t) p];
            float& v = eff[(size_t) p];
            if (t.flags & pfInt) v = clampParam (p, v + offs[i].amt * (t.max - t.min));
            else v = fromNorm (p, toNorm (p, v) + offs[i].amt);
        }
    }

    float lfoValue (int k, const VoiceCtx& c)
    {
        const int b = k == 0 ? LFO1_SHAPE : LFO2_SHAPE;
        const float depth = base (b + 7);
        const int dest = (int) (base (b + 6) + 0.5f);
        if (dest == mdNone || std::fabs (depth) < 1e-6f) return 0.0f;
        const int shape = (int) (base (b) + 0.5f), sync = (int) (base (b + 2) + 0.5f), mode = (int) (base (b + 3) + 0.5f);
        const float fade = base (b + 4), ph0 = base (b + 5);
        double phase;
        const double tAbs = (double) c.absSample / c.fs;
        if (sync > 0)
        {
            const double beats = divisionBeats (sync);
            phase = (mode == 0 || mode == 2) ? c.ppq / beats : noteTime * c.bpm / 60.0 / beats;
        }
        else
        {
            const double rate = base (b + 1);
            phase = (mode == 0 || mode == 2) ? tAbs * rate : noteTime * rate;
        }
        if (mode == 3 && phase >= 1.0) phase = 0.9999;
        phase += ph0;
        const double fl = std::floor (phase); const float p = (float) (phase - fl);
        float v;
        switch (shape)
        {
            case 1: v = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; break;
            case 2: v = 2.0f * p - 1.0f; break;
            case 3: v = fastTanh (sinT (p) * 12.0f); break;
            case 4: v = hashBi (77u + (uint32_t) k, (uint64_t) (int64_t) fl, (uint64_t) track); break;
            case 5:
            {
                const float a = hashBi (91u + (uint32_t) k, (uint64_t) (int64_t) fl, (uint64_t) track), bb = hashBi (91u + (uint32_t) k, (uint64_t) ((int64_t) fl + 1), (uint64_t) track);
                const float w = 0.5f - 0.5f * cosT (0.5f * p);
                v = a + (bb - a) * w; break;
            }
            default: v = sinT (p); break;
        }
        if (mode == 2) { if (! lfoLatched[k]) { lfoHeld[k] = v; lfoLatched[k] = true; } v = lfoHeld[k]; }
        float d = depth;
        if (fade > 0.0f) d *= (float) std::min (1.0, noteTime / fade);
        else if (fade < 0.0f) d *= (float) std::max (0.0, 1.0 - noteTime / -fade);
        return v * d;
    }

    float modSource (int s, const VoiceCtx& c, float l1, float l2) const
    {
        switch (s)
        {
            case msVelocity: return vel;
            case msKey: return (pitchCur - 60.0f) / 48.0f;
            case msWheel: return c.wheel;
            case msAftertouch: return c.aftertouch;
            case msPoly: return polyPressure;
            case msBend: return c.bend;
            case msLfo1: return l1;
            case msLfo2: return l2;
            case msRandom: return randomMod;
            case msBreath: return c.breath;
            case msExpression: return c.expression;
            default: return 0.0f;
        }
    }

public:
    float polyPressure = 0.0f;

private:
    void addOff (int dest, float amt)
    {
        if (dest == mdPitch) vPitch += amt;
        else if (dest == mdAmp) vAmp += amt;
        else if (dest == mdIndex) vIndex += amt;
        else if (dest == mdPanSpread) vPan += amt;
        else if (dest >= kDestParamBase && dest < kDestParamBase + kNumSoundParams && nOffs < 12) offs[nOffs++] = { dest, amt };
    }

    // Slow-changing configuration (envelope coefficients, switches). `fresh` at note start.
    void configure (const VoiceCtx& c, bool fresh)
    {
        refreshMod (c);
        configureRest (c, fresh);
    }
    // Modulation of this period (LFOs and matrix): every control period, P() needs it.
    void refreshMod (const VoiceCtx& c)
    {
        nOffs = 0; vPitch = vAmp = vIndex = vPan = 0.0f;
        const float l1 = lfoValue (0, c), l2 = lfoValue (1, c);
        addOff ((int) (base (LFO1_DEST) + 0.5f), l1);
        addOff ((int) (base (LFO2_DEST) + 0.5f), l2);
        for (int m = 0; m < 8; ++m)
        {
            const int b = MOD1_SRC + 3 * m;
            const int src = (int) (base (b) + 0.5f);
            const float amt = base (b + 2);
            if (src == msNone || amt == 0.0f) continue;
            addOff ((int) (base (b + 1) + 0.5f), modSource (src, c, l1, l2) * amt);
        }
        buildEff();
    }
    void configureRest (const VoiceCtx& c, bool fresh)
    {
        algo = std::clamp ((int) (P (ALGO) + 0.5f), 0, 7);
        { const Algo& al = algos()[(size_t) algo]; for (int d = 0; d < 4; ++d) { modMask[d] = 0; for (int j = d + 1; j < 4; ++j) if (al.mod[j][d]) modMask[d] |= 1 << j; } }
        vintage = P (VINTAGE) > 0.5f;
        const bool isKit = lane > 0;
        const float tsKit = isKit ? c.kitDecayScale : 1.0f;
        for (int o = 0; o < 4; ++o)
        {
            const float ts = P (opParam (o, oTScale));
            env[o].set (P (opParam (o, oAtt)) * ts, P (opParam (o, oDec)) * ts * tsKit, P (opParam (o, oSus)), P (opParam (o, oRel)) * ts * tsKit,
                        P (opParam (o, oDelay)), P (opParam (o, oCurve)), P (opParam (o, oLoop)) > 0.5f, (float) fs);
        }
        fenvE.set (P (F_ATT), P (F_DEC) * tsKit, P (F_SUS), P (F_REL) * tsKit, 0.0f, 0.0f, false, (float) fs);
        amp.set (P (AMP_ATT), P (AMP_DEC) * tsKit, P (AMP_SUS), P (AMP_REL) * tsKit, 0.0f, 0.0f, false, (float) fs);
        ftype = std::clamp ((int) (P (FTYPE) + 0.5f), 0, 6);
        basscomp = P (BASSCOMP);
        fenvOct = P (FENV) * 7.0f;
        const float nl = P (NOISE_LEVEL);
        noiseOn = nl > 0.0f;
        if (noiseOn)
        {
            noiseMode = std::clamp ((int) (P (NOISE_MODE) + 0.5f), 0, 2);
            noiseFollowsAmp = P (NOISE_ENV) > 0.5f;
            noiseSvf.set (std::min (P (NOISE_FREQ), (float) (0.45 * fs2)), P (NOISE_Q), (float) fs2);
            noiseDecay = std::exp (-6.9077553f / (std::max (P (NOISE_DECAY) * tsKit, 0.002f) * (float) fs2));
            const float na = P (NOISE_ATT);
            noiseRampInc = na > 0.0f ? 1.0f / (na * (float) fs2) : 1.0f;
        }
        const float base_ = P (BASE), width = P (WIDTH);
        spanHp = base_ > 11.0f; spanLp = width < 21900.0f;
        spanHpA = std::exp (-kTwoPi * base_ / (float) fs2); spanLpA = std::exp (-kTwoPi * std::min (width, (float) (0.45 * fs2)) / (float) fs2);
        if (qs) { qs->spHpOn[ql] = spanHp ? 1.0f : 0.0f; qs->spLpOn[ql] = spanLp ? 1.0f : 0.0f; qs->spHpA[ql] = spanHpA; qs->spLpA[ql] = spanLpA; }
        const float cr = P (CRUSH);
        crushOn = cr > 0.001f;
        if (crushOn) { crushN = 1 + (int) (cr * cr * 30.0f); crushQ = std::exp2 (15.0f - 11.0f * cr); }
        subOn = P (SUB_LEVEL) > 0.0f && ! isKit;
        subTri = P (SUB_SHAPE) > 0.5f;
        (void) fresh;
    }

    // Targets of the continuous values; interpolated over `samples` host samples (2·samples at 2x).
    void targets (const VoiceCtx& c, int samples)
    {
        const bool isKit = lane > 0;
        const float age = std::max (P (AGE), isKit ? c.kitAge : 0.0f);
        // Pitch: glide toward the target.
        const float glide = P (GLIDE);
        const bool glideAlways = P (GLIDE_MODE) > 0.5f;
        if (glide > 0.0005f && (legatoGlide || glideAlways))
        {
            const float k = 1.0f - std::exp (-(float) samples / (float) fs / (glide * 0.33f));
            pitchCur += (pitchTarget - pitchCur) * k;
            if (std::fabs (pitchTarget - pitchCur) < 0.001f) { pitchCur = pitchTarget; legatoGlide = false; }
        }
        else { pitchCur = pitchTarget; legatoGlide = false; }
        const double tSec = (double) c.absSample / c.fs;
        float cents = detune + P (FINE);
        if (age > 0.0f)
        {
            const double p1 = ageF1 * tSec + ageP1, p2 = ageF2 * tSec + ageP2;
            cents += age * (5.0f * sinT ((float) (p1 - std::floor (p1))) + 2.0f * sinT ((float) (p2 - std::floor (p2))) + 3.0f * ageNoteCents);
        }
        // Pitch envelope (sweep), at control rate.
        const float pdepth = P (PENV_DEPTH);
        float sweep = 0.0f;
        if (pdepth != 0.0f) { sweep = pdepth * penv; penv *= std::exp (-(float) samples / (float) fs / std::max (P (PENV_TIME), 0.0005f)); }
        const float bend = c.bend * P (BEND);
        float pitch = pitchCur + P (TUNE) + bend + cents * 0.01f + vPitch * 12.0f;
        if (isKit) pitch += 0.0f;
        const float f0 = mtof (pitch + sweep);
        const float sweepMul = semis (sweep);
        const float lim = (float) (0.5 * fs2);
        float fop[4];
        for (int o = 0; o < 4; ++o)
        {
            const float fixed = P (opParam (o, oFixed));
            const float dcent = P (opParam (o, oDetune)) + age * 2.5f * ageOp[o];
            fop[o] = fixed > 0.0f ? fixed * sweepMul * semis (dcent * 0.01f) : f0 * P (opParam (o, oRatio)) * semis (dcent * 0.01f);
            fop[o] = std::min (fop[o], (float) (0.45 * fs2));
            tgt[cInc0 + o] = fop[o] / (float) fs2;
        }
        const Algo& al = algos()[(size_t) algo];
        const float noteHz = std::max (mtof (pitch), 20.0f);
        for (int o = 0; o < 4; ++o)
        {
            const float L = P (opParam (o, oLevel));
            const float vs = P (opParam (o, oVel));
            const float velg = (1.0f - vs) + vs * vel * std::sqrt (vel);
            const float ks = P (opParam (o, oKs));
            const float ksg = ks == 0.0f ? 1.0f : std::pow (noteHz / 261.6f, -ks);
            opOn[o] = L > 0.0f;
            if (al.car[o]) tgt[cAmp0 + o] = L * velg * (ks != 0.0f ? std::min (ksg, 4.0f) : 1.0f);
            else
            {
                float idx = 10.0f * L * L * velg * std::min (ksg, 4.0f) * std::max (0.0f, 1.0f + vIndex);
                // Index limit per destination: Carson (f_d + (I + 1) f_mod < fs2/2) and the Bessel tail: the first
                // sideband that would fold back into the audio band after the 2x stage (above 0.71·fs2) has order
                // k_f = (0.71·fs2 − f_d) / f_mod; J_k(I) is negligible (< −90 dB) for k > 1.5·I + 6, so I ≤ (k_f − 6) / 1.5.
                for (int d = 0; d < 4; ++d)
                    if (al.mod[o][d])
                    {
                        const float fm = std::max (fop[o], 1.0f);
                        const float carson = (lim - fop[d]) / fm - 1.0f;
                        const float bessel = ((0.71f * (float) fs2 - fop[d]) / fm - 6.0f) / 1.5f;
                        idx = std::min (idx, std::max (0.0f, std::min (carson, bessel)));
                    }
                tgt[cAmp0 + o] = idx;
                // A modulator with nothing to modulate is silent (still computed for feedback chains? no: skip).
                bool feeds = false; for (int d = 0; d < 4; ++d) feeds |= al.mod[o][d];
                if (! feeds) opOn[o] = false;
            }
            const float fb = P (opParam (o, oFb));
            float fl = 1200.0f / std::max (fop[o], 1.0f);
            fl = fl >= 1.0f ? 1.0f : std::max (std::sqrt (fl), 0.25f);
            fbAmt[o] = fb;
            tgt[cFb0 + o] = 1.5f * fb * fl;
            // Harmonics morph.
            const float ha = P (opParam (o, oHAmt));
            const int hs = std::clamp ((int) (P (opParam (o, oHShape)) + 0.5f), 0, 5);
            hOn[o] = ha > 0.0f && hs > 0;
            if (hOn[o])
            {
                const auto& S = harmShapes()[(size_t) hs];
                const float limH = al.car[o] ? 18000.0f : 30000.0f;
                for (int k = 0; k < 8; ++k)
                {
                    const float w = (k + 1) * fop[o] > limH ? 0.0f : S.w[k] / S.peak;
                    hw[o][k] = (k == 0 ? (1.0f - ha) : 0.0f) + ha * w;
                }
            }
        }
        // Filter cutoff in octaves (log2 Hz), keytrack around C4, AGE spread.
        const float cut = P (CUTOFF);
        tgt[cCutOct] = std::log2 (cut) + P (KTRK) * (pitch - 60.0f) / 12.0f + age * 0.25f * ageCut;
        tgt[cReso] = P (RESO);
        // Output gain: patch level, velocity, AGE level spread, amp modulation, KIT group gain.
        const float va = P (VEL_AMP);
        float g = dbToGain (P (LEVEL)) * ((1.0f - va) + va * vel) * (1.0f + age * 0.08f * ageLev) * std::max (0.0f, 1.0f + vAmp);
        if (isKit) g *= c.groupGain[laneGroup (lane)];
        tgt[cGain] = g;
        // Drive: 0..36 dB, asymmetric bias, level compensation. Above F#4 the highest drive and fold gains are capped,
        // the cap falling 6 dB per octave: the clipper's and the folder's harmonics reach about gain × pitch, and the
        // 4x stage has room for that only up to there (max drive + fold + feedback: C6 −30 and C7 −25 dB of alias
        // energy before, −44 and −53 dB now). Moderate settings are below the cap and sound as before.
        const float d = P (DRIVE);
        driveOn = d > 0.001f;
        const float hiOct = std::max (0.0f, (pitch - 66.0f) / 12.0f);
        const float dg = dbToGain (std::min (d * 36.0f, 36.0f - 6.0f * hiOct));
        tgt[cDrive] = dg; tgt[cBias] = 0.22f * d;
        tgt[cDriveComp] = (float) (0.5 / std::max (0.05, clipF (0.5 * dg)));
        const float f = P (FOLD);
        foldOn = f > 0.001f;
        tgt[cFold] = 1.0f + 5.0f * std::min (f, std::exp2 (-hiOct));
        tgt[cNoise] = P (NOISE_LEVEL);
        const float so = P (SUB_OCT) > 0.5f ? 0.25f : 0.5f;
        tgt[cSubInc] = noteHz * so / (float) fs;
        tgt[cSubGain] = P (SUB_LEVEL) * dbToGain (P (LEVEL)) * ((1.0f - va) + va * vel);
        tgt[cPan] = clampf (P (PAN) + panOff + vPan, -1.0f, 1.0f);
        tgt[cSendD] = P (SEND_DELAY); tgt[cSendR] = P (SEND_REVERB);
        tgt[cGl] = cosT ((tgt[cPan] + 1.0f) * 0.125f); tgt[cGr] = sinT ((tgt[cPan] + 1.0f) * 0.125f);
        // Live changes (knobs, lock-only trigs) glide over ~6 ms instead of jumping in one control period.
        if (! firstTargets)
        {
            const float ks = 1.0f - std::exp (-(float) samples / (0.006f * (float) fs));
            for (int k : { (int) cCutOct, (int) cReso, (int) cGain, (int) cDrive, (int) cDriveComp, (int) cFold, (int) cPan, (int) cSubGain, (int) cGl, (int) cGr }) tgt[k] = cur[k] + (tgt[k] - cur[k]) * ks;
        }
        firstTargets = false;
        // Interpolation increments (per 2x sample).
        const float n2 = (float) std::max (1, 2 * samples);
        for (int k = 0; k < kNumCur; ++k) inc[k] = (tgt[k] - cur[k]) / n2;
        // Pitch increments jump less audibly than they ramp for big sweeps: fine either way at 0.33 ms.
        fadeLen = (float) (0.005 * fs2);
    }

    static int laneGroup (int lane) { static const int g[9] = { 0, 0, 1, 1, 2, 2, 0, 3, 3 }; return g[std::clamp (lane, 0, 8)]; }
};

} // namespace dh::dv
