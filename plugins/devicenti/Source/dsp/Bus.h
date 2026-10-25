#pragma once
// DeVicenti: what happens on a track bus after the voices (plan §5.3, §13.1, §13.2). Per track, at the host rate:
//
//   FM layer (stereo) → INSERT → AGE tape stage → [sends are taken here] ─┐
//   SUB layer (mono, delayed to stay aligned) ─────────────────────────────┴→ low-end mono below 120 Hz → fader
//
// The sub never passes through the insert or the tape: the clean weight under a distorted bass.
//
// AGE tape stage: a modulated read of a short delay (wow 0.52 Hz, flutter 6.3 Hz, slow random drift, all closed
// formulas of the absolute sample position, so a bar sounds the same wherever playback starts), two poles of HF
// loss, soft saturation and a trace of hiss. The base delay (2 ms) is always there, so the plugin latency does not
// change with AGE.
//
// Inserts: OVERDRIVE/FOLD, BITCRUSH/LO-FI (bits, sample-and-hold rate tied to the position, anti-alias filters
// switchable: off is the grainy sampler colour), CHORUS/ENSEMBLE, PHASER, RING MOD, COMB/RESONATOR (tuned to the
// pattern root, with a limiter in the loop so a runaway comb cannot blow up).

#include "Base.h"

namespace dh::dv {

// ───────────── AGE tape stage ─────────────
class TapeStage
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate; base = (int) std::lround (0.002 * fs);
        relSig = std::exp (-1.0f / (2.0f * (float) fs));
        for (auto& l : line) l.prepare (base * 2 + 16);
        reset();
    }
    void reset() { for (auto& l : line) l.clear(); for (auto& p : lp) p.reset(); ageSm = 0.0f; sigEnv = 0.0f; hiss.seed (0x1234567u); }
    int latency() const { return base; }
    // In place. pos = absolute sample of the first sample; seed = per-track phase offset.
    void process (float* L, float* R, int n, float age, int64_t pos, uint32_t seed)
    {
        const float k = 1.0f - std::exp (-1.0f / (0.05f * (float) fs));
        const double w = 0.52, f = 6.3, offW = hashUni (seed, 1, 3), offF = hashUni (seed, 2, 3);
        // Per call (a call never crosses a 16-sample control period of the absolute timeline): the HF-loss coefficient
        // from the AGE of this segment, and the slow drift interpolated across the control period (position-based:
        // the same samples whatever the block size).
        const float fcA = 20000.0f - 13000.0f * age;
        const float gA = 1.0f - std::exp (-kTwoPi * std::min (fcA, 0.45f * (float) fs) / (float) fs);
        const int64_t p0 = pos - (pos % 16);
        const float dr0 = drift (p0, seed), dr1 = drift (p0 + 16, seed);
        // Wow and flutter are slow (0.52 and 6.3 Hz): their sines at the two ends of the control period, interpolated.
        auto wave = [&] (int64_t s, double hz, double off) { const double ph = hz * (double) s / fs + off; return sinT ((float) (ph - std::floor (ph))); };
        const float w0 = wave (p0, w, offW), w1 = wave (p0 + 16, w, offW), f0 = wave (p0, f, offF), f1 = wave (p0 + 16, f, offF);
        for (int i = 0; i < n; ++i)
        {
            ageSm += (age - ageSm) * k;
            const float a = ageSm;
            line[0].push (L[i]); line[1].push (R[i]);
            if (a < 1.0e-4f) { L[i] = line[0].at (base + 1); R[i] = line[1].at (base + 1); continue; }
            // Peak speed error 0.4 % (wow) and 0.1 % (flutter) at AGE 1, as delay excursions p/(2πf)·fs.
            const float wowS = (float) (0.004 / (2.0 * 3.14159265358979323846 * w) * fs) * a;
            const float flS = (float) (0.001 / (2.0 * 3.14159265358979323846 * f) * fs) * a;
            const float u = (float) (pos + i - p0) * (1.0f / 16.0f);
            const float dr = dr0 + (dr1 - dr0) * u;
            const float m = wowS * (w0 + (w1 - w0) * u) + flS * (f0 + (f1 - f0) * u) + dr * 0.35f * wowS;
            const float d = (float) base + 1.0f - m;
            float l = line[0].read (d), r = line[1].read (d);
            // HF loss: two poles from 20 kHz down to 7 kHz at AGE 1.
            const float g = gA;
            lp[0].z += g * (l - lp[0].z); lp[1].z += g * (lp[0].z - lp[1].z); l = lp[1].z;
            lp[2].z += g * (r - lp[2].z); lp[3].z += g * (lp[2].z - lp[3].z); r = lp[3].z;
            const float sd = 1.0f + 1.2f * a;
            l = fastTanh (l * sd) / sd; r = fastTanh (r * sd) / sd;
            const float lvl = std::max (std::fabs (l), std::fabs (r));
            sigEnv = lvl > sigEnv ? sigEnv + (lvl - sigEnv) * 0.01f : sigEnv * relSig;
            const float hs = 1.3e-4f * a * std::min (1.0f, sigEnv * 40.0f);     // the hiss follows the programme
            L[i] = l + hs * hiss.bi();
            R[i] = r + hs * hiss.bi();
        }
    }
private:
    double fs = 48000.0; int base = 96;
    Line line[2]; OnePole lp[4]; float ageSm = 0.0f, sigEnv = 0.0f, relSig = 0.9999f; Noise hiss;
    // Slow random drift (−1..1): cosine interpolation between hashed values every 0.4 s.
    float drift (int64_t s, uint32_t seed) const
    {
        const double cell = (double) s / (0.4 * fs);
        const int64_t c0 = (int64_t) std::floor (cell);
        const float u = (float) (cell - (double) c0);
        const float a = hashBi (seed, (uint64_t) c0, 5), b = hashBi (seed, (uint64_t) (c0 + 1), 5);
        return a + (b - a) * (0.5f - 0.5f * cosT (0.5f * u));
    }
};

// ───────────── low-end mono: the side signal high-passed at 120 Hz (2 poles) ─────────────
struct LowMono
{
    Svf side;
    void prepare (double fs) { side.set (120.0f, 0.707f, (float) fs); side.reset(); }
    void reset() { side.reset(); }
    inline void process (float& l, float& r)
    {
        const float m = 0.5f * (l + r), s = side.highpass (0.5f * (l - r));
        l = m + s; r = m - s;
    }
};

// ───────────── per-track insert ─────────────
enum InsertType : int { kInsOff, kInsDrive, kInsLofi, kInsChorus, kInsPhaser, kInsRing, kInsComb };

struct InsertParams { int type = 0; float amt = 0.5f, p1 = 0.5f, p2 = 0.5f, mix = 1.0f; float rootHz = 65.4f; };

class Insert
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (auto& l : line) l.prepare ((int) (0.06f * fs));
        for (auto& l : comb) l.prepare ((int) (fs / 15.0f) + 8);
        reset();
    }
    void reset()
    {
        for (auto& l : line) l.clear();
        for (auto& l : comb) l.clear();
        for (auto& s : pre) s.reset(); for (auto& s : post) s.reset(); for (auto& s : tone) s.reset();
        for (auto& a : ap) a.fill (0.0f);
        phFb[0] = phFb[1] = 0; held[0] = held[1] = 0; lofiCell = -1; combLp[0] = combLp[1] = 0; combRms = 0; combGr = 1.0f; dcx[0] = dcx[1] = dcy[0] = dcy[1] = 0; lastType = -1;
        smAmt = smP1 = smP2 = smMix = -1.0f;
    }

    void process (float* L, float* R, int n, const InsertParams& p, int64_t pos)
    {
        if (p.type != lastType) { if (lastType >= 0) reset(); lastType = p.type; smAmt = p.amt; smP1 = p.p1; smP2 = p.p2; smMix = p.mix; }
        if (p.type == kInsOff) return;
        const float k = 1.0f - std::exp (-1.0f / (0.02f * fs));
        for (int i = 0; i < n; ++i)
        {
            smAmt += (p.amt - smAmt) * k; smP1 += (p.p1 - smP1) * k; smP2 += (p.p2 - smP2) * k; smMix += (p.mix - smMix) * k;
            float l = L[i], r = R[i];
            float wl = l, wr = r;
            switch (p.type)
            {
                case kInsDrive: driveFold (wl, wr); break;
                case kInsLofi: lofi (wl, wr, pos + i, p); break;
                case kInsChorus: chorus (wl, wr, pos + i); break;
                case kInsPhaser: phaser (wl, wr, pos + i); break;
                case kInsRing: ring (wl, wr, pos + i); break;
                case kInsComb: combRes (wl, wr, p.rootHz); break;
                default: break;
            }
            L[i] = l + (wl - l) * smMix; R[i] = r + (wr - r) * smMix;
        }
    }

private:
    float fs = 48000.0f;
    Line line[2], comb[2];
    Svf pre[4], post[4], tone[2];
    std::array<float, 6> ap[2] {};
    float phFb[2] {}, held[2] {}, combLp[2] {}, combRms = 0, combGr = 1.0f, dcx[2] {}, dcy[2] {};
    int lastType = -1;
    float smAmt = 0, smP1 = 0, smP2 = 0, smMix = 0;
    int64_t lofiCell = -1;

    inline void driveFold (float& l, float& r)
    {
        const float g = dbToGain (smAmt * 30.0f), fold = smP2;
        const float comp = 1.0f / (0.6f + 0.4f * std::sqrt (g));
        auto sh = [&] (float x, int c) {
            float y = fastTanh (g * x + 0.15f * smAmt) - fastTanh (0.15f * smAmt);
            if (fold > 0.02f) { const float u = y * (1.0f + 3.0f * fold); y = y + fold * ((float) foldF (u) - y); }
            const float dy = y - dcx[c] + 0.9995f * dcy[c]; dcx[c] = y; dcy[c] = dy;
            return dy * comp;
        };
        const float fc = 900.0f * fastExp2 (smP1 * 4.3f);
        tone[0].set (fc, 0.6f, fs); tone[1].set (fc, 0.6f, fs);
        l = tone[0].lowpass (sh (l, 0)); r = tone[1].lowpass (sh (r, 1));
    }

    inline void lofi (float& l, float& r, int64_t s, const InsertParams& p)
    {
        // Bits 16 → 4 over the amount (≈ 12 bits at 1/3); rate 44.1 kHz → 2.7 kHz over p1 (≈ 24 kHz at 0.22).
        const float bits = 16.0f - 12.0f * smAmt;
        const float q = std::exp2 (bits - 1.0f);
        const double rate = std::min ((double) fs, 44100.0 * std::exp2 (-4.0 * smP1));
        const bool aa = p.p2 >= 0.5f;
        if (aa) { const float fc = (float) (0.42 * rate); for (int c = 0; c < 2; ++c) { pre[c].set (fc, 0.54f, fs); pre[c + 2].set (fc, 1.31f, fs); post[c].set (fc, 0.54f, fs); post[c + 2].set (fc, 1.31f, fs); } }
        float in[2] = { l, r };
        if (aa) for (int c = 0; c < 2; ++c) in[c] = pre[c + 2].lowpass (pre[c].lowpass (in[c]));
        // Sample-and-hold clock as a function of the absolute position: the same samples are held in any block size.
        const int64_t cell = (int64_t) std::floor ((double) s * rate / fs);
        if (cell != lofiCell) { lofiCell = cell; for (int c = 0; c < 2; ++c) held[c] = std::floor (in[c] * q + 0.5f) / q; }
        float o[2] = { held[0], held[1] };
        if (aa) for (int c = 0; c < 2; ++c) o[c] = post[c + 2].lowpass (post[c].lowpass (o[c]));
        l = o[0]; r = o[1];
    }

    inline void chorus (float& l, float& r, int64_t s)
    {
        const double t = (double) s / fs;
        const double rate = 0.05 * std::exp2 (smP1 * 7.3);           // 0.05 … 8 Hz
        const bool ens = smP2 >= 0.5f;
        line[0].push (l); line[1].push (r);
        const float depth = (0.0004f + 0.0045f * smAmt) * fs, ctr = (ens ? 0.009f : 0.007f) * fs;
        if (! ens)
        {
            const float a = sinT ((float) frac1 (rate * t)), b = sinT ((float) frac1 (rate * t + 0.25));
            const float wl = line[0].read (ctr + depth * a), wr = line[1].read (ctr + depth * b);
            l = 0.7f * l + 0.7f * wl; r = 0.7f * r + 0.7f * wr;
        }
        else
        {
            float wl = 0, wr = 0;
            for (int k = 0; k < 3; ++k)
            {
                const float ph = (float) k / 3.0f;
                const float m = sinT ((float) frac1 (rate * t + ph)) + 0.18f * sinT ((float) frac1 (6.0 * t + ph));
                wl += line[0].read (ctr + depth * m); wr += line[1].read (ctr + depth * -m);
            }
            l = 0.6f * l + 0.45f * wl; r = 0.6f * r + 0.45f * wr;
        }
    }

    inline void phaser (float& l, float& r, int64_t s)
    {
        const double t = (double) s / fs;
        const double rate = 0.02 * std::exp2 (smP1 * 8.0);           // 0.02 … 5 Hz
        float io[2] = { l, r };
        for (int c = 0; c < 2; ++c)
        {
            const float u = 0.5f + 0.5f * sinT ((float) frac1 (rate * t + 0.25 * c));
            const float fc = 160.0f * fastExp2 (smAmt * 6.0f * u);
            const float tt = dh::prewarp (fc, fs), a = (tt - 1.0f) / (tt + 1.0f);
            float v = io[c] + phFb[c] * 0.85f * smP2;
            for (auto& z : ap[c]) { const float y = a * v + z; z = v - a * y; v = y; }
            phFb[c] = fastTanh (v);
            io[c] = 0.5f * (io[c] + v) * 1.4f;
        }
        l = io[0]; r = io[1];
    }

    inline void ring (float& l, float& r, int64_t s)
    {
        const double hz = 5.0 * std::exp2 (smP1 * 8.65);              // 5 Hz … 2 kHz
        const float ph = (float) frac1 ((double) s / fs * hz);
        const float sq = fastTanh (sinT (ph) * (1.0f + 20.0f * smP2));
        const float c = sinT (ph) + (sq - sinT (ph)) * smP2;
        l = l + (l * c - l) * smAmt; r = r + (r * c - r) * smAmt;
    }

    inline void combRes (float& l, float& r, float rootHz)
    {
        // p1: ±24 semitones around the root; amount: feedback up to 0.95; p2: damping.
        const float hz = clampf (rootHz * semis ((smP1 - 0.5f) * 48.0f), 20.0f, 4000.0f);
        const float fb = 0.95f * smAmt * combGr;
        const float damp = 0.05f + 0.85f * smP2;
        float io[2] = { l, r };
        for (int c = 0; c < 2; ++c)
        {
            const float d = fs / (hz * (c == 0 ? 1.0f : 1.0017f));
            const float y = comb[c].read (d);
            combLp[c] += (y - combLp[c]) * (1.0f - damp);
            const float v = fastTanh (io[c] * 0.5f + fb * combLp[c]);
            comb[c].push (v);
            io[c] = y;
            combRms += (y * y - combRms) * 0.0005f;
        }
        // Stability: above −6 dBFS RMS the loop gain is pulled down (fast), then recovers slowly.
        const float rms = std::sqrt (combRms);
        combGr = rms > 0.5f ? std::max (0.3f, combGr * 0.995f) : std::min (1.0f, combGr + 0.00002f);
        l = io[0] * 0.8f; r = io[1] * 0.8f;
    }
};

} // namespace dh::dv
