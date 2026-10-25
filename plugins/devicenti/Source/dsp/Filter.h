#pragma once
// DeviceNTI V: the per-voice filter (runs at the voice's 2x rate).
//   LP24: a four-pole TPT ladder. The feedback is solved in closed form on the linear model (no unit delay), the
//         input/feedback sum goes through tanh and every stage output through a soft saturation, so a hard drive
//         rounds off like a transistor ladder instead of clipping. Resonance 0..1 maps to k 0..3.9 (self-oscillation
//         at the top), the prototype's curve. Bass compensation 0..1 adds input gain (1 + c·k) so turning the resonance up does not thin
//         the low end. Each voice gets a small fixed spread of the stage cutoffs and of k (component tolerance).
//   LP12, BP, HP, NOTCH, PEAK: the TPT state-variable filter with a soft-saturated band state.
//   OFF: straight through.
// The SPAN pre-filter (a 6 dB high-pass "base" and 6 dB low-pass "width") sits before it.

#include "Base.h"

namespace dh::dv {

enum FilterType : int { kLP24, kLP12, kBP, kHP, kNotch, kPeak, kFilterOff };

class VoiceFilter
{
public:
    void setSpread (uint32_t seed)
    {
        for (int i = 0; i < 4; ++i) spread[i] = 1.0f + 0.025f * hashBi (seed, (uint64_t) i, 11);
        kSpread = 1.0f + 0.03f * hashBi (seed, 9, 12);
    }
    void reset() { s1 = s2 = s3 = s4 = 0; svf.reset(); }

    // Coefficients (g = tan(π fc / fs), res 0..1): the voice refreshes them every few samples.
    inline void setCoefs (int type, float g0, float res)
    {
        ctype = type; cres = res;
        if (type == kLP24)
        {
            k = 3.9f * res * kSpread;                                      // the prototype's curve: 3.9 self-oscillates
            for (int i = 0; i < 4; ++i) { const float g = g0 * spread[i]; G[i] = g / (1.0f + g); }
            Gt = G[0] * G[1] * G[2] * G[3];
        }
        else if (type != kFilterOff)
        {
            svf.setG (g0, 2.0f - 1.96f * res);                             // damping 2 → 0.04 (Q 0.5 → 25), as the prototype
        }
    }
    inline float process (float x, float comp)
    {
        switch (ctype)
        {
            case kLP24: return ladder (x, comp);
            case kFilterOff: return x;
            default: break;
        }
        svf.tick (x);
        svf.ic1 = fastTanh (svf.ic1 * 0.4f) * 2.5f;                        // gentle analog-style band saturation
        switch (ctype)
        {
            case kLP12: return svf.lp;
            case kBP: return svf.bp * svf.k * (1.0f + 0.5f * cres);
            case kHP: return svf.hp;
            case kNotch: return svf.lp + svf.hp;
            default: return x + svf.bp * svf.k * (1.0f + 6.0f * cres);     // peak: a resonant bump on top of the input
        }
    }

    // The ladder's coefficients, for the four-voice renderer (which runs the ladder itself, four lanes at once).
    bool isLadder() const { return ctype == kLP24; }
    float stageSpread (int i) const { return spread[i]; }
    float resSpread() const { return kSpread; }
    void ladderCoefs (float* g4, float& gt, float& kk) const { for (int i = 0; i < 4; ++i) g4[i] = G[i]; gt = Gt; kk = k; }

private:
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    float spread[4] { 1, 1, 1, 1 }, kSpread = 1.0f;
    float G[4] { 0.5f, 0.5f, 0.5f, 0.5f }, Gt = 0.0625f, k = 0.0f, cres = 0.0f; int ctype = kLP24;
    Svf svf;

    inline float ladder (float x, float comp)
    {
        // Linear estimate of the output: y4 = Gt·u + S, with S the state contributions through the cascade.
        const float S = ((s1 * (1.0f - G[0]) * G[1] + s2 * (1.0f - G[1])) * G[2] + s3 * (1.0f - G[2])) * G[3] + s4 * (1.0f - G[3]);
        const float in = x * (1.0f + comp * k);
        const float y4e = (Gt * in + S) / (1.0f + k * Gt);
        const float u = fastTanh (in - k * y4e);
        float v = (u - s1) * G[0]; float o1 = v + s1; s1 = o1 + v; o1 = softStage (o1);
        v = (o1 - s2) * G[1]; float o2 = v + s2; s2 = o2 + v; o2 = softStage (o2);
        v = (o2 - s3) * G[2]; float o3 = v + s3; s3 = o3 + v;
        v = (o3 - s4) * G[3]; const float o4 = v + s4; s4 = o4 + v;
        return o4;
    }
    // Per-stage saturation: transparent at small levels, rounding off above ~0.7 (cheap rational curve).
    static inline float softStage (float v) { const float a = std::fabs (v); return a < 0.7f ? v : std::copysign (0.7f + 0.3f * fastTanh ((a - 0.7f) / 0.3f), v); }
};

// The SPAN pre-filter: one-pole high-pass (base) then one-pole low-pass (width). Coefficients by the caller.
struct Span
{
    float hpZ = 0, lpZ = 0;
    void reset() { hpZ = lpZ = 0; }
    inline float process (float x, float hpA, float lpA, bool hpOn, bool lpOn)
    {
        if (hpOn) { hpZ = x + hpA * (hpZ - x); x -= hpZ; }
        if (lpOn) { lpZ = x + lpA * (lpZ - x); x = lpZ; }
        return x;
    }
};

} // namespace dh::dv
