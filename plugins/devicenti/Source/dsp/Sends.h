#pragma once
// DeVicenti: the two send effects, once per instance (plan §5.6, §13.2).
//
// DELAY, three modes:
//   TAPE       one tape loop with wow and flutter on the read, high-pass, tone low-pass and soft saturation in the
//              feedback; the time glides like a tape machine when it changes (pitch bend instead of a click).
//   DUB        band-passed, saturating feedback that self-oscillates above ~95 %; FREEZE holds the loop (input cut,
//              unity feedback). Feedback and send can be thrown per step (step locks), smoothed so nothing clicks.
//   PING-PONG  cross-fed left/right, tone in the loop.
// REVERB, four modes:
//   SPRING     a spring model of our own: three dispersive delay loops (cascades of first-order all-passes tuned by
//              frequency, so the chirp is the same at any sample rate) for the drip and the "boing", decay control.
//   PLATE      the Dattorro plate from shared/dsp.
//   HALL, VOID Halation's 8-line FDN (dh::sp::Network). VOID is very long (to 30 s), dark and modulated.
//   FREEZE     holds the tail (HALL/VOID: the network freezes; SPRING/PLATE: a second network catches the send).
//   SHIMMER    Halation's octave-up two-head shifter in the network loop (HALL/VOID).
// Delay into reverb: an amount of the delay's output feeds the reverb.

#include "Base.h"
#include "Fx.h"                                         // shared/dsp: dh::PlateReverb
#include "../../../halation/Source/dsp/Network.h"       // dh::sp::Network
#include "../../../halation/Source/dsp/Modules.h"       // dh::sp::Shimmer

namespace dh::dv {

enum DelayMode : int { kDelayTape, kDelayDub, kDelayPingPong };
enum ReverbMode : int { kRevSpring, kRevPlate, kRevHall, kRevVoid };

struct DelayParams { int mode = kDelayTape; float timeS = 0.5f, feedback = 0.4f, tone = 0.5f, wow = 0.2f, ret = 1.0f; bool freeze = false; };
struct ReverbParams { int mode = kRevHall; float size = 0.5f, decayS = 3.0f, damp = 0.5f, predelayS = 0.01f, mod = 0.3f, shimmer = 0.0f, ret = 1.0f; bool freeze = false, freezeArmed = true; };

class Delay
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (auto& l : line) l.prepare ((int) (4.2f * fs));
        reset();
    }
    void reset()
    {
        for (auto& l : line) l.clear();
        for (auto& s : bp) s.reset(); for (auto& s : lp) s.reset(); for (auto& h : hp) h.reset();
        dSm = -1.0f; fbSm = -1.0f; frz = 0.0f; fbState[0] = fbState[1] = 0.0f;
    }
    // In place: L/R in = send bus, out = wet.
    void process (float* L, float* R, int n, const DelayParams& p, int64_t pos)
    {
        const float dTarget = clampf (p.timeS * fs, 8.0f, 4.0f * fs);
        if (dSm < 0.0f) dSm = dTarget;
        float fbT = p.mode == kDelayDub ? clampf (p.feedback, 0.0f, 1.12f) : clampf (p.feedback, 0.0f, 0.95f);
        if (fbSm < 0.0f) fbSm = fbT;
        const float glide = 1.0f - std::exp (-1.0f / (0.15f * fs)), fbK = 1.0f - std::exp (-1.0f / (0.03f * fs)), frzK = 1.0f - std::exp (-1.0f / (0.05f * fs));
        const float toneHz = 1200.0f * fastExp2 (p.tone * 3.9f);   // 1.2 … 18 kHz
        lp[0].set (toneHz, 0.6f, fs); lp[1].set (toneHz, 0.6f, fs);
        hp[0].setHz (p.mode == kDelayTape ? 110.0f : 60.0f, fs); hp[1].setHz (p.mode == kDelayTape ? 110.0f : 60.0f, fs);
        // Dub: the band-pass of the loop (centre follows the tone, Q a little resonant).
        const float bpHz = 350.0f * fastExp2 (p.tone * 3.2f);
        bp[0].set (bpHz, 0.9f, fs); bp[1].set (bpHz, 0.9f, fs);
        for (int i = 0; i < n; ++i)
        {
            dSm += (dTarget - dSm) * glide;
            fbSm += ((p.freeze && p.mode == kDelayDub ? 1.0f : fbT) - fbSm) * fbK;
            frz += ((p.freeze && p.mode == kDelayDub ? 1.0f : 0.0f) - frz) * frzK;
            const double t = (double) (pos + i) / fs;
            const float wow = p.wow * (0.004f * sinT ((float) frac1 (0.55 * t)) + 0.001f * sinT ((float) frac1 (6.1 * t)));
            const float d = dSm * (1.0f + (p.mode == kDelayPingPong ? 0.25f : 1.0f) * wow);
            const float yl = line[0].read (d), yr = line[1].read (p.mode == kDelayTape ? d * 1.0137f : d);
            const float in = 0.5f * (L[i] + R[i]) * (1.0f - frz);
            switch (p.mode)
            {
                case kDelayDub:
                {
                    // Band-passed, driven loop: the gain at the band centre is the feedback, tanh holds a runaway.
                    float fl = bp[0].bandpass (yl), fr = bp[1].bandpass (yr);
                    fl = fastTanh (1.3f * (in + fbSm * fl)) / 1.3f;
                    fr = fastTanh (1.3f * (in * 0.6f + fbSm * fr)) / 1.3f;
                    line[0].push (fl); line[1].push (fr);
                    L[i] = yl; R[i] = 0.7f * yr + 0.3f * yl;
                    break;
                }
                case kDelayPingPong:
                {
                    const float fl = lp[0].lowpass (hp[0].hp (in + fbSm * yr));
                    const float fr = lp[1].lowpass (hp[1].hp (fbSm * yl));
                    line[0].push (fastTanh (fl)); line[1].push (fastTanh (fr));
                    L[i] = yl; R[i] = yr;
                    break;
                }
                default:
                {
                    float v = in + fbSm * 0.5f * (yl + yr);
                    v = fastTanh (1.25f * v) / 1.25f;
                    v = lp[0].lowpass (hp[0].hp (v));
                    line[0].push (v); line[1].push (v);
                    L[i] = yl; R[i] = yr;
                    break;
                }
            }
            L[i] *= p.ret; R[i] *= p.ret;
        }
    }
private:
    float fs = 48000.0f, dSm = -1.0f, fbSm = -1.0f, frz = 0.0f, fbState[2] {};
    Line line[2]; Svf bp[2], lp[2]; OnePole hp[2];
};

// Spring: a dispersive delay loop (cascade of first-order all-passes in the loop, like a spring's frequency-dependent
// group delay) with a low-pass, a DC block and a decay set by the loop gain.
class SpringTank
{
public:
    static constexpr int kAp = 24;
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        const float ms[3] = { 33.7f, 41.3f, 47.9f };
        for (int s = 0; s < 3; ++s) { len[s] = ms[s] * 1e-3f * fs; line[s].prepare ((int) len[s] + 16); }
        // The all-pass corner sits near 4.5 kHz whatever the rate: the "drip" chirp keeps its pitch.
        for (int s = 0; s < 3; ++s)
        {
            const double fc = (s == 0 ? 4200.0 : (s == 1 ? 4700.0 : 3900.0));
            const double t = std::tan (3.14159265358979323846 * std::min (fc, 0.45 * fs) / fs);
            apA[s] = (float) ((1.0 - t) / (1.0 + t));
        }
        reset();
    }
    void reset() { for (auto& l : line) l.clear(); for (auto& s : st) { s.x.fill (0.0f); s.y.fill (0.0f); s.lp = 0; s.dcx = 0; s.dcy = 0; } }
    inline void process (float in, float decayS, float tone, float& outL, float& outR)
    {
        if (decayS != lastDecay) { lastDecay = decayS; for (int s = 0; s < 3; ++s) gain[s] = std::pow (10.0f, -3.0f * (len[s] / fs) / std::max (0.2f, decayS)); }
        float o[3];
        for (int s = 0; s < 3; ++s)
        {
            const float g = gain[s];
            float v = line[s].read (len[s]);
            o[s] = v;
            // Dispersion.
            auto& S = st[s];
            for (int i = 0; i < kAp; ++i) { const float y = -apA[s] * v + S.x[(size_t) i] + apA[s] * S.y[(size_t) i]; S.x[(size_t) i] = v; S.y[(size_t) i] = y; v = y; }
            S.lp += (v - S.lp) * (0.25f + 0.6f * tone);
            const float dc = S.lp - S.dcx + 0.995f * S.dcy; S.dcx = S.lp; S.dcy = dc;
            line[s].push (fastTanh (in * 0.6f + g * dc));
        }
        outL = 0.8f * (o[0] + 0.6f * o[1]); outR = 0.8f * (o[2] + 0.6f * o[1]);
    }
private:
    float fs = 48000.0f, len[3] {}, apA[3] {}, gain[3] {}, lastDecay = -1.0f;
    Line line[3];
    struct St { std::array<float, kAp> x {}, y {}; float lp = 0, dcx = 0, dcy = 0; } st[3];
};

class Reverb
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        net.prepare (fs, 11u, 600.0f);
        freezer.prepare (fs, 23u, 400.0f);
        plate.prepare (fs);
        spring.prepare (fs);
        shimmer.prepare (fs); shimmer.setInterval (0);
        pre[0].prepare ((int) (0.25f * fs)); pre[1].prepare ((int) (0.25f * fs));
        tmpL.assign (4096, 0.0f); tmpR.assign (4096, 0.0f);
        reset();
    }
    void reset()
    {
        net.reset(); freezer.reset(); plate.restart(); spring.reset(); shimmer.reset(); pre[0].clear(); pre[1].clear();
        lastMode = -1; frzSm = 0.0f; shL = shR = 0.0f; hpL.reset(); hpR.reset(); configured = false;
    }

    void process (float* L, float* R, int n, const ReverbParams& p)
    {
        for (int i0 = 0; i0 < n; i0 += 4096) chunk (L + i0, R + i0, std::min (4096, n - i0), p);
    }

private:
    float fs = 48000.0f;
    dh::sp::Network net, freezer;
    dh::PlateReverb plate;
    SpringTank spring;
    dh::sp::Shimmer shimmer;
    Line pre[2];
    OnePole hpL, hpR;
    std::vector<float> tmpL, tmpR;
    int lastMode = -1; bool configured = false;
    float frzSm = 0.0f, shL = 0.0f, shR = 0.0f;
    ReverbParams last;

    void configure (const ReverbParams& p)
    {
        dh::sp::NetParams np;
        np.space = 1.0f; np.width = 1.0f; np.lowCutHz = 30.0f;
        if (p.mode == kRevVoid)
        {
            np.room = dh::sp::RoomType::hall; np.size = 2.0f + 2.0f * p.size; np.decayS = clampf (p.decayS, 4.0f, 30.0f);
            np.hfRatio = 0.06f + 0.3f * (1.0f - p.damp); np.lfRatio = 1.3f; np.diffusion = 1.0f; np.modDepth = 0.4f + 0.6f * p.mod; np.modRate = 0.25f;
        }
        else
        {
            np.room = dh::sp::RoomType::hall; np.size = 0.6f + 1.2f * p.size; np.decayS = clampf (p.decayS, 0.4f, 12.0f);
            np.hfRatio = 0.15f + 0.7f * (1.0f - p.damp); np.lfRatio = 1.0f; np.diffusion = 0.8f; np.modDepth = 0.15f + 0.5f * p.mod; np.modRate = 0.4f;
        }
        np.freezeMask = (p.freeze && (p.mode == kRevHall || p.mode == kRevVoid)) ? 7 : 0;
        net.set (np);
        dh::sp::NetParams fz = np; fz.room = dh::sp::RoomType::hall; fz.size = 1.4f; fz.decayS = 6.0f; fz.freezeMask = p.freeze ? 7 : 0;
        freezer.set (fz);
        last = p; configured = true;
    }

    void chunk (float* L, float* R, int n, const ReverbParams& p)
    {
        if (p.mode != lastMode) { if (lastMode >= 0) { net.reset(); plate.restart(); spring.reset(); } lastMode = p.mode; configured = false; }
        if (! configured || p.size != last.size || p.decayS != last.decayS || p.damp != last.damp || p.mod != last.mod || p.freeze != last.freeze) configure (p);
        hpL.setHz (120.0f, fs); hpR.setHz (120.0f, fs);
        const float predelay = clampf (p.predelayS * fs, 1.0f, 0.24f * fs);
        const float frzK = 1.0f - std::exp (-1.0f / (0.08f * fs));
        const bool netMode = p.mode == kRevHall || p.mode == kRevVoid;
        // Input: pre-delay and a gentle high-pass (keeps the tail out of the sub).
        for (int i = 0; i < n; ++i)
        {
            pre[0].push (L[i] - hpL.lp (L[i])); pre[1].push (R[i] - hpR.lp (R[i]));
            tmpL[(size_t) i] = pre[0].read (predelay); tmpR[(size_t) i] = pre[1].read (predelay);
        }
        if (netMode)
        {
            for (int i = 0; i < n; ++i)
            {
                // Shimmer: the previous wet output, an octave up, injected into the loop.
                if (p.shimmer > 0.001f) { const float s = shimmer.process (0.5f * (shL + shR)); net.injectL = net.injectR = s * p.shimmer * 0.45f; }
                else net.injectL = net.injectR = 0.0f;
                float oL, oR;
                net.process (tmpL[(size_t) i], tmpR[(size_t) i], oL, oR);
                shL = oL; shR = oR;
                L[i] = oL * p.ret; R[i] = oR * p.ret;
            }
        }
        else if (p.mode == kRevPlate)
        {
            for (int i = 0; i < n; ++i) { L[i] = tmpL[(size_t) i]; R[i] = tmpR[(size_t) i]; }
            plate.process (tmpL.data(), tmpR.data(), n, clampf (p.decayS / 5.0f, 0.05f, 1.0f), p.damp, 1.0f);
            for (int i = 0; i < n; ++i) { const float yl = tmpL[(size_t) i] - L[i], yr = tmpR[(size_t) i] - R[i]; L[i] = yl * 1.2f * p.ret; R[i] = yr * 1.2f * p.ret; }
        }
        else
        {
            for (int i = 0; i < n; ++i)
            {
                float oL, oR;
                spring.process (0.5f * (tmpL[(size_t) i] + tmpR[(size_t) i]) * (0.7f + 0.6f * p.size), std::max (0.6f, p.decayS), 1.0f - p.damp, oL, oR);
                L[i] = oL * p.ret; R[i] = oR * p.ret;
            }
        }
        // FREEZE for SPRING / PLATE: a second network catches the send while freeze rises and holds it.
        // It listens all the time (unfrozen) so that, when FREEZE engages, its lines already hold the recent send;
        // only when the project can freeze (the setting is on or a step locks it): otherwise it costs CPU for nothing.
        if (! netMode && (p.freezeArmed || p.freeze || frzSm > 1e-4f))
            for (int i = 0; i < n; ++i)
            {
                frzSm += ((p.freeze ? 1.0f : 0.0f) - frzSm) * frzK;
                float oL, oR;
                freezer.process (tmpL[(size_t) i] * 0.5f, tmpR[(size_t) i] * 0.5f, oL, oR);
                L[i] += oL * frzSm * p.ret; R[i] += oR * frzSm * p.ret;
            }
    }
};

} // namespace dh::dv
